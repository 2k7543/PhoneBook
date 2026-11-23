FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    g++ \
    qt6-base-dev \
    qt6-svg-dev \
    libsqlite3-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    ca-certificates \
    libcpp-httplib-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY CMakeLists.txt ./
COPY Include/ Include/
COPY src/ src/
COPY app/qt/ app/qt/
COPY app/server/ app/server/
COPY app/web/ app/web/
COPY tests/ tests/

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target PhoneBookWebServer --parallel

RUN groupadd --system phonebook \
    && useradd --system --gid phonebook phonebook \
    && mkdir -p /app/data \
    && chown phonebook:phonebook /app/data

ENV PHONEBOOK_WEB_ROOT=/app/app/web \
    PHONEBOOK_DB_PATH=/app/data/phonebook.db \
    PORT=8080

USER phonebook

EXPOSE 8080

CMD ["/app/build/PhoneBookWebServer"]