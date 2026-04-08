from django.urls import path
from . import views

urlpatterns = [
    path("register/", views.user_register),
    path("login/", views.user_login),
    path("dashboard/", views.dashboard),
    path("dashboard/create-folder/", views.create_folder_dashboard),
    path("dashboard/folder/<int:folder_id>/", views.folder_view),
    path("dashboard/folder/<int:folder_id>/create-subfolder/", views.create_subfolder),
    path("dashboard/folder/<int:folder_id>/create-file/", views.create_file),
    path("dashboard/folder/<int:folder_id>/file/<int:file_id>/", views.file_view),
]
