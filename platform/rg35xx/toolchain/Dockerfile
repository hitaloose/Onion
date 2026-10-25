# Build environment for the Onion RG35XX rootfs (Buildroot) and Onion binaries.
# Buildroot needs a case-sensitive filesystem: keep its build dir in a Docker
# volume, not in a bind mount from macOS.
#   docker build -t rg35xx-toolchain platform/rg35xx/toolchain
FROM debian:bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
        bc bison build-essential ca-certificates cpio curl file flex git \
        libncurses-dev python3 rsync unzip wget xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Some host packages refuse to configure as root; build as a regular user
RUN useradd -m builder && mkdir -p /work && chown builder /work
USER builder
WORKDIR /work
