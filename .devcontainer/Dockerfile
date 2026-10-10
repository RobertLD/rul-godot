FROM mcr.microsoft.com/devcontainers/cpp:1-ubuntu-24.04

# Ubuntu 24.04 ships libstdc++ 13, which has the constexpr std::vector support that
# libstdc++ 11 lacks and that rul::collections::matrix::Matrix's defaulted operator== needs.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    clang \
    clang-format \
    clang-tidy \
    clangd \
    lld \
    ninja-build \
    python3 \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

ENV CC=clang
ENV CXX=clang++

# tests/integration/app.py and render.py need these; a venv satisfies PEP 668.
ENV VIRTUAL_ENV=/opt/venv
ENV PATH="${VIRTUAL_ENV}/bin:${PATH}"
RUN python3 -m venv "${VIRTUAL_ENV}" \
    && "${VIRTUAL_ENV}/bin/pip" install --no-cache-dir dash pillow \
    && chown -R vscode:vscode "${VIRTUAL_ENV}"
