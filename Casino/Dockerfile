FROM python:3.11-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    libargon2-dev \
    && rm -rf /var/lib/apt/lists/*

RUN pip install --no-cache-dir "argon2-cffi[ffi]"==21.3.0

WORKDIR /app

COPY . .

RUN ls -la

EXPOSE 8000

CMD ["python", "main.py"]