# Build environment for the RG35XX kernel (rg35xx-cfw/s500_kernel, Linux 3.10).
# Linux 3.10 does not build with current GCC; Ubuntu 14.04 ships GCC 4.7.3
# (Linaro), the same compiler the Garlic kernel was built with. It is amd64
# only, so on Apple Silicon this image runs under emulation.
#   docker build --platform linux/amd64 -t rg35xx-kernel-toolchain platform/rg35xx/kernel/toolchain
FROM ubuntu:14.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        bc ca-certificates gcc git gcc-4.7-arm-linux-gnueabihf libc6-dev libncurses5-dev \
        lzop make perl u-boot-tools xz-utils \
    && ln -s arm-linux-gnueabihf-gcc-4.7 /usr/bin/arm-linux-gnueabihf-gcc \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m builder && mkdir -p /work && chown builder /work
USER builder
WORKDIR /work
