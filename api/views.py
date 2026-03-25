from django.shortcuts import render, redirect
from django.contrib.auth.models import User
from django.contrib.auth import authenticate, login
from django.contrib.auth.decorators import login_required
from .models import Folder, File

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

#when u redirect to dashboard page this runs and fetches the folders
@login_required
def dashboard(request):
    user = request.user
    folders = Folder.objects.filter(user = user)
    return render(request, "dashboard.html", {"folders": folders})
