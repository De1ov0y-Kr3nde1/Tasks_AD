FROM ubuntu:22.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        g++ \
        libsqlite3-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

ENV TZ=Europe/Moscow
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

WORKDIR /app

COPY . .

RUN g++ -std=c++17 main.cpp data/db.cpp -lsqlite3 -lpthread -o main

RUN mkdir -p data

EXPOSE 8080

CMD ["./main"]