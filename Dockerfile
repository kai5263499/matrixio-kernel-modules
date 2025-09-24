# Multi-architecture kernel module builder for Matrix Creator
FROM ubuntu:22.04

# Prevent interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install base dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    crossbuild-essential-armhf \
    crossbuild-essential-arm64 \
    bc \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    dwarves \
    kmod \
    cpio \
    device-tree-compiler \
    debhelper \
    dkms \
    devscripts \
    fakeroot \
    wget \
    curl \
    git \
    rsync \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# Set up working directory
WORKDIR /workspace

# Copy build scripts
COPY docker/ /scripts/
RUN chmod +x /scripts/*.sh

# Entry point script
ENTRYPOINT ["/scripts/build.sh"]
