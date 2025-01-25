FROM python:3.12

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY ./app /app/app
COPY config.yml /app/config.yml

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]