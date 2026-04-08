from django.db import models
from django.contrib.auth.models import User

from django.db import models
from django.contrib.auth.models import User

class Folder(models.Model):
    name = models.CharField(max_length=100)
    user = models.ForeignKey(User, on_delete=models.CASCADE)
    parent = models.ForeignKey(
        'self',
        on_delete=models.CASCADE,
        null=True,
        blank=True,
        related_name='subfolders'
    )

    class Meta:
        constraints = [
            models.UniqueConstraint(
                fields=['name', 'parent', 'user'],
                name='unique_folder_per_parent_per_user'
            )
        ]
class File(models.Model):
    name = models.CharField(max_length=100)
    content = models.TextField(blank=True, default="")
    folder = models.ForeignKey(Folder, on_delete=models.CASCADE)
    user = models.ForeignKey(User, on_delete=models.CASCADE)

    class Meta:
        constraints = [
            models.UniqueConstraint(
                fields=['name', 'folder', 'user'],
                name='unique_file_per_folder_per_user'
            )
        ]