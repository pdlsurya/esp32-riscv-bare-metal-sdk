FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG RISCV_TOOLCHAIN_VERSION=esp-14.2.0_20251107
ARG RISCV_TOOLCHAIN_ARCHIVE=riscv32-esp-elf-14.2.0_20251107-x86_64-linux-gnu.tar.xz
ARG RISCV_TOOLCHAIN_URL=https://github.com/espressif/crosstool-NG/releases/download/${RISCV_TOOLCHAIN_VERSION}/${RISCV_TOOLCHAIN_ARCHIVE}

RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    cmake \
    file \
    git \
    make \
    ninja-build \
    python3 \
    python3-pip \
    python3-venv \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN wget -O /tmp/riscv32-esp-elf.tar.xz "${RISCV_TOOLCHAIN_URL}" \
    && mkdir -p /opt/espressif \
    && tar -xJf /tmp/riscv32-esp-elf.tar.xz -C /opt/espressif \
    && rm -f /tmp/riscv32-esp-elf.tar.xz

RUN python3 -m venv /opt/espressif/python \
    && /opt/espressif/python/bin/pip install --no-cache-dir --upgrade pip \
    && /opt/espressif/python/bin/pip install --no-cache-dir esptool

ENV PATH="/opt/espressif/python/bin:/opt/espressif/riscv32-esp-elf/bin:${PATH}"

WORKDIR /work

CMD ["/bin/bash"]
