# Generated manually to store editable file contents.

from django.db import migrations, models


class Migration(migrations.Migration):

    dependencies = [
        ("api", "0002_rename_filename_file_name_alter_file_unique_together_and_more"),
    ]

    operations = [
        migrations.AddField(
            model_name="file",
            name="content",
            field=models.TextField(blank=True, default=""),
        ),
    ]