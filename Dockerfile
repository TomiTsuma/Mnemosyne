# Mnemosyne — full application image (C++ server + CLI client + ML runtime)
#
# Build:
#   docker build -t mnemosyne:latest .
#
# Run:
#   docker run --rm -p 1143:1143 -p 4311:4311 mnemosyne:latest
#   curl http://localhost:1143/ping
#
# Compose:
#   docker compose up -d
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
    && cmake --build build \
         --target mnemosyne_server mnemosyne_client \
         -j"$(nproc)" \
    && mkdir -p /opt/mnemosyne-libs \
    && for bin in /src/build/bin/mnemosyne_server /src/build/bin/mnemosyne_client; do \
         ldd "$bin" \
           | awk '/=> \// { print $3 }' \
           | while read -r lib; do \
                case "$lib" in \
                  */ld-linux-x86-64.so.2|*/libc.so.6|*/libm.so.6|*/libpthread.so.0|*/libdl.so.2|*/librt.so.1|*/libresolv.so.2) \
                    ;; \
                  *) cp -L "$lib" /opt/mnemosyne-libs/ ;; \
                esac; \
             done; \
       done

# ------------------------------------------------------------------------------
# Python — ML runtime virtualenv (sklearn, xgboost, optuna, …)
# ------------------------------------------------------------------------------
FROM python:3.12-slim-bookworm AS python-deps

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/*

RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

COPY ml_runtime/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir --upgrade pip \
    && pip install --no-cache-dir -r /tmp/requirements.txt

# ------------------------------------------------------------------------------
# Runtime — C++ binaries + Python ML backend + web UI
# ------------------------------------------------------------------------------
FROM python:3.12-slim-bookworm AS runtime

LABEL org.opencontainers.image.title="Mnemosyne" \
      org.opencontainers.image.description="Column-oriented analytical DBMS with ML runtime"

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        libgomp1 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --home /app --create-home --shell /usr/sbin/nologin mnemosyne

# GCC 14 C++ runtime from the builder (bookworm default libstdc++ is too old).
COPY --from=builder /opt/mnemosyne-libs/ /usr/local/lib/
ENV LD_LIBRARY_PATH=/usr/local/lib

# Static wget for health checks (python-slim has no curl/wget by default).
COPY --from=busybox:1.36 /bin/wget /usr/local/bin/wget

# C++ server and CLI client.
COPY --from=builder /src/build/bin/mnemosyne_server /usr/local/bin/mnemosyne_server
COPY --from=builder /src/build/bin/mnemosyne_client /usr/local/bin/mnemosyne_client

# Python ML runtime and its installed packages.
COPY --from=python-deps /opt/venv /opt/venv
COPY ml_runtime ./ml_runtime
COPY public ./public

ENV PATH="/opt/venv/bin:$PATH" \
    MNEMO_PYTHON=/opt/venv/bin/python \
    MNEMO_ML_RUNTIME=/app/ml_runtime/run.py \
    MNEMO_MODELS_DIR=/app/mnemo_models

WORKDIR /app

RUN mkdir -p /tmp/mnemosyne /app/mnemo_models \
    && chown -R mnemosyne:mnemosyne /app /tmp/mnemosyne

VOLUME ["/tmp/mnemosyne", "/app/mnemo_models"]

USER mnemosyne

EXPOSE 1143 4311

HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=3 \
  CMD wget -qO- http://127.0.0.1:1143/ping || exit 1

ENTRYPOINT ["mnemosyne_server"]
