from django.shortcuts import render, redirect
from django.contrib.auth.models import User
from django.contrib.auth import authenticate, login
from django.contrib.auth.decorators import login_required
from django.conf import settings
from django.http import HttpResponseForbidden
from .models import Folder, File
from pathlib import Path
import subprocess
import tempfile
import shutil
import os
import re


def home(request):
    return render(request, "home.html")

#takes input username, pass, cnfrm pass :
#   if correct redirects to dashboard page
#   if wrong returns a the request obj with an additional field error
def user_register(request):
    if request.method == "POST":
        username = request.POST.get("username")
        password = request.POST.get("password")
        confirm_password = request.POST.get("confirm_password")

        if password != confirm_password:
            return render(request, "register.html", {"error": "Passwords do not match"})

        if User.objects.filter(username=username).exists():
            return render(request, "register.html", {"error": "Username already exists"})

        user = User.objects.create_user(
            username=username,
            password=password
        )

        login(request, user)

        return redirect("/dashboard/")

    return render(request, "register.html")

#takes input username, pass :
#   if correct redirects to dashboard page
#   if wrong returns a the request obj with an additional field error
def user_login(request):
    if request.method == "POST":
        username = request.POST.get("username")
        password = request.POST.get("password")

        user = authenticate(request, username=username, password=password)

        if user is not None:
            login(request, user)
            return redirect("/dashboard/")
        else:
            return render(request, "login.html", {"error": "Invalid username or password"})

    return render(request, "login.html")

#helper function to build breadcrumb path
def get_folder_path(folder):
    path = []
    current = folder
    while current:
        path.insert(0, current)
        current = current.parent
    return path


def get_runtime_compiler_path():
    compiler_env_path = os.environ.get("COMPILER_PATH")
    compiler_path = Path(compiler_env_path) if compiler_env_path else (Path(settings.BASE_DIR) / "compiler")

    if not compiler_path.exists():
        return None, f"Compiler binary not found at {compiler_path}"

    # In serverless deployments, executing files directly from the app bundle can fail.
    # Copying to /tmp and executing there is more reliable.
    if os.environ.get("VERCEL"):
        runtime_compiler_path = Path("/tmp") / "compiler-runtime"
        try:
            shutil.copy2(compiler_path, runtime_compiler_path)
            runtime_compiler_path.chmod(0o755)
            return runtime_compiler_path, None
        except OSError as exc:
            return None, f"Failed to prepare runtime compiler: {exc}"

    try:
        compiler_path.chmod(0o755)
    except OSError:
        pass

    return compiler_path, None


ANSI_ESCAPE_RE = re.compile(r"\x1B\[[0-?]*[ -/]*[@-~]")


def normalize_process_output(stdout_text, stderr_text):
    combined = (stdout_text or "") + (stderr_text or "")
    cleaned = ANSI_ESCAPE_RE.sub("", combined).strip()
    return cleaned or "No compiler output was returned."

#when u redirect to dashboard page this runs and fetches the folders
@login_required
def dashboard(request):
    user = request.user
    folders = Folder.objects.filter(user = user, parent = None)
    return render(request, "dashboard.html", {"folders": folders})

#view files and subfolders in a specific folder
@login_required
def folder_view(request, folder_id):
    user = request.user
    folder = Folder.objects.filter(user = user, id = folder_id).first()
    
    if not folder:
        return redirect("/dashboard/")
    
    # Get subfolders and files in this folder
    subfolders = Folder.objects.filter(parent = folder, user = user)
    files = File.objects.filter(folder = folder, user = user)
    breadcrumb = get_folder_path(folder)
    
    return render(request, "folder_view.html", {
        "folder": folder, 
        "subfolders": subfolders, 
        "files": files,
        "breadcrumb": breadcrumb
    })

#view a specific file
@login_required
def file_view(request, folder_id, file_id):
    user = request.user
    folder = Folder.objects.filter(user = user, id = folder_id).first()
    
    if not folder:
        return redirect("/dashboard/")
    
    file = File.objects.filter(folder = folder, id = file_id, user = user).first()
    
    if not file:
        return redirect(f"/dashboard/folder/{folder_id}/")

    is_psa = file.name.lower().endswith(".psa")

    if request.method == "POST":
        action = request.POST.get("action", "save")
        editor_content = request.POST.get("content", "")
        stdin_value = request.POST.get("stdin", "")

        file.content = editor_content
        file.save()

        if action == "save":
            if not is_psa:
                return redirect(f"/dashboard/folder/{folder_id}/file/{file_id}/")

            return render(request, "file_view.html", {
                "folder": folder,
                "file": file,
                "breadcrumb": get_folder_path(folder),
                "is_psa": is_psa,
                "editor_content": editor_content,
                "stdin_value": stdin_value,
                "output": "Saved.",
            })

        if not is_psa:
            return HttpResponseForbidden("Run is only available for .psa files")

        compiler_path, compiler_error = get_runtime_compiler_path()
        if compiler_error:
            return render(request, "file_view.html", {
                "folder": folder,
                "file": file,
                "breadcrumb": get_folder_path(folder),
                "is_psa": is_psa,
                "editor_content": editor_content,
                "stdin_value": stdin_value,
                "output": "",
                "error": compiler_error,
            })

        with tempfile.TemporaryDirectory() as temp_dir:
            source_path = Path(temp_dir) / file.name
            output_path = Path(temp_dir) / "output"
            source_path.write_text(editor_content, encoding="utf-8")

            try:
                compile_process = subprocess.run(
                    [str(compiler_path), str(source_path), "-o", str(output_path)],
                    capture_output=True,
                    text=True,
                    cwd=str(settings.BASE_DIR),
                )
            except OSError as exc:
                return render(request, "file_view.html", {
                    "folder": folder,
                    "file": file,
                    "breadcrumb": get_folder_path(folder),
                    "is_psa": is_psa,
                    "editor_content": editor_content,
                    "stdin_value": stdin_value,
                    "output": "",
                    "error": f"Compiler could not be executed: {exc}",
                })

            if compile_process.returncode != 0:
                compile_output = normalize_process_output(
                    compile_process.stdout,
                    compile_process.stderr,
                )
                return render(request, "file_view.html", {
                    "folder": folder,
                    "file": file,
                    "breadcrumb": get_folder_path(folder),
                    "is_psa": is_psa,
                    "editor_content": editor_content,
                    "stdin_value": stdin_value,
                    "output": compile_output,
                    "error": f"Compilation failed (exit code {compile_process.returncode})",
                })

            try:
                output_path.chmod(0o755)
                run_process = subprocess.run(
                    [str(output_path)],
                    input=stdin_value,
                    capture_output=True,
                    text=True,
                    cwd=str(temp_dir),
                    timeout=30,
                )
                output_text = normalize_process_output(
                    run_process.stdout,
                    run_process.stderr,
                )
            except subprocess.TimeoutExpired:
                output_text = "Program timed out after 30 seconds."
            except OSError as exc:
                output_text = ""
                return render(request, "file_view.html", {
                    "folder": folder,
                    "file": file,
                    "breadcrumb": get_folder_path(folder),
                    "is_psa": is_psa,
                    "editor_content": editor_content,
                    "stdin_value": stdin_value,
                    "output": output_text,
                    "error": f"Compiled output could not be executed: {exc}",
                })

            return render(request, "file_view.html", {
                "folder": folder,
                "file": file,
                "breadcrumb": get_folder_path(folder),
                "is_psa": is_psa,
                "editor_content": editor_content,
                "stdin_value": stdin_value,
                "output": output_text,
            })
    
    breadcrumb = get_folder_path(folder)
    return render(request, "file_view.html", {
        "folder": folder, 
        "file": file,
        "breadcrumb": breadcrumb
        ,"is_psa": is_psa,
        "editor_content": file.content,
        "stdin_value": "",
    })

#create a new folder at dashboard level
@login_required
def create_folder_dashboard(request):
    if request.method == "POST":
        folder_name = request.POST.get("folder_name")
        user = request.user
        
        if folder_name and not Folder.objects.filter(user=user, name=folder_name, parent=None).exists():
            Folder.objects.create(name=folder_name, user=user, parent=None)
            return redirect("/dashboard/")
        else:
            folders = Folder.objects.filter(user=user, parent=None)
            return render(request, "dashboard.html", {
                "folders": folders, 
                "error": "Folder name already exists or is empty"
            })
    
    return redirect("/dashboard/")

#create a new subfolder within a folder
@login_required
def create_subfolder(request, folder_id):
    if request.method == "POST":
        subfolder_name = request.POST.get("folder_name")
        user = request.user
        parent_folder = Folder.objects.filter(user=user, id=folder_id).first()
        
        if not parent_folder:
            return redirect("/dashboard/")
        
        if subfolder_name and not Folder.objects.filter(user=user, name=subfolder_name, parent=parent_folder).exists():
            Folder.objects.create(name=subfolder_name, user=user, parent=parent_folder)
            return redirect(f"/dashboard/folder/{folder_id}/")
        else:
            subfolders = Folder.objects.filter(parent=parent_folder, user=user)
            files = File.objects.filter(folder=parent_folder, user=user)
            breadcrumb = get_folder_path(parent_folder)
            return render(request, "folder_view.html", {
                "folder": parent_folder,
                "subfolders": subfolders,
                "files": files,
                "breadcrumb": breadcrumb,
                "error": "Folder name already exists or is empty"
            })
    
    return redirect(f"/dashboard/folder/{folder_id}/")

#create a new file within a folder
@login_required
def create_file(request, folder_id):
    if request.method == "POST":
        file_name = request.POST.get("file_name")
        user = request.user
        folder = Folder.objects.filter(user=user, id=folder_id).first()
        
        if not folder:
            return redirect("/dashboard/")
        
        if file_name and not File.objects.filter(user=user, name=file_name, folder=folder).exists():
            File.objects.create(name=file_name, user=user, folder=folder)
            return redirect(f"/dashboard/folder/{folder_id}/")
        else:
            subfolders = Folder.objects.filter(parent=folder, user=user)
            files = File.objects.filter(folder=folder, user=user)
            breadcrumb = get_folder_path(folder)
            return render(request, "folder_view.html", {
                "folder": folder,
                "subfolders": subfolders,
                "files": files,
                "breadcrumb": breadcrumb,
                "error": "File name already exists or is empty"
            })
    
    return redirect(f"/dashboard/folder/{folder_id}/")


