# Mnemosyne DBMS — multi-stage image for Docker Hub
#
# Build:
#   docker build -t YOUR_DOCKERHUB_USER/mnemosyne:latest .
#
# Push:
#   docker login
#   docker push YOUR_DOCKERHUB_USER/mnemosyne:latest
#
# Run:
#   docker run --rm -p 1143:1143 YOUR_DOCKERHUB_USER/mnemosyne:latest
#   curl http://localhost:1143/ping
#
# DNS troubleshooting (if apt fails with "Temporary failure resolving …"):
#   Docker Desktop → Settings → Docker Engine → merge this, Apply & Restart:
#   "dns": ["8.8.8.8", "1.1.1.1"]

# ------------------------------------------------------------------------------
# Builder — GCC 14 (preinstalled) + CMake 3.28+ (bookworm apt ships 3.25)
# ------------------------------------------------------------------------------
FROM gcc:14-bookworm AS builder

ARG CMAKE_VERSION=3.28.3

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        git \
    && rm -rf /var/lib/apt/lists/* \
    && curl -fsSL \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz" \
        | tar xz -C /opt \
    && ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake

WORKDIR /src
COPY . .

RUN cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTS=OFF \
      -DENABLE_BENCHMARK=OFF \
    && cmake --build build --target mnemosyne_server -j"$(nproc)" \
    && mkdir -p /opt/mnemosyne-libs \
    && ldd /src/build/bin/mnemosyne_server \
       | awk '/=> \// { print $3 }' \
       | while read -r lib; do \
            case "$lib" in \
              */ld-linux-x86-64.so.2|*/libc.so.6|*/libm.so.6|*/libpthread.so.0|*/libdl.so.2|*/librt.so.1|*/libresolv.so.2) \
                ;; \
              *) cp -L "$lib" /opt/mnemosyne-libs/ ;; \
            esac; \
         done

# ------------------------------------------------------------------------------
# Runtime — slim Debian, no apt (avoids DNS issues in this build stage)
# ------------------------------------------------------------------------------
FROM debian:bookworm-slim AS runtime

LABEL org.opencontainers.image.title="Mnemosyne" \
      org.opencontainers.image.description="Column-oriented analytical DBMS"

# GCC 14 C++ runtime from the builder (bookworm default libstdc++ is too old).
COPY --from=builder /opt/mnemosyne-libs/ /usr/local/lib/
ENV LD_LIBRARY_PATH=/usr/local/lib

# Static wget for health checks (no curl/apt in runtime).
COPY --from=busybox:1.36 /bin/wget /usr/local/bin/wget

WORKDIR /app

COPY --from=builder /src/build/bin/mnemosyne_server /usr/local/bin/mnemosyne_server
COPY public ./public

RUN mkdir -p /tmp/mnemosyne \
    && useradd --system --home /app --shell /usr/sbin/nologin mnemosyne \
    && chown -R mnemosyne:mnemosyne /app /tmp/mnemosyne

USER mnemosyne

EXPOSE 1143 4311

HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
  CMD wget -qO- http://127.0.0.1:1143/ping || exit 1

ENTRYPOINT ["mnemosyne_server"]
