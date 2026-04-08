FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1

WORKDIR /app

COPY requirements.txt /app/
RUN pip install --no-cache-dir -r requirements.txt

COPY . /app/

# Ensure compiler binary is executable inside container.
RUN chmod +x /app/compiler || true

EXPOSE 10000

CMD sh -c "python manage.py migrate && gunicorn config.wsgi:application --bind 0.0.0.0:${PORT:-10000}"