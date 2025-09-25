#!/bin/bash
set -e

# Default values - handle both KERNEL_TAG (legacy) and KERNEL_VERSION (new)
ARCH=${ARCH:-arm}
KERNEL_VERSION=${KERNEL_VERSION:-${KERNEL_TAG:-stable_20240529}}
KERNEL_CONFIG=${KERNEL_CONFIG:-bcmrpi_defconfig}
TARGET_PLATFORM=${TARGET_PLATFORM:-generic}

echo "=== Matrix Creator Kernel Module Builder ==="
echo "Architecture: $ARCH"
echo "Kernel Version: $KERNEL_VERSION"  
echo "Kernel Config: $KERNEL_CONFIG"
echo "Target Platform: $TARGET_PLATFORM"

# Set cross-compilation variables
if [ "$ARCH" = "arm64" ]; then
    export CROSS_COMPILE=aarch64-linux-gnu-
    export DEB_ARCH=arm64
else
    export CROSS_COMPILE=arm-linux-gnueabihf-
    export DEB_ARCH=armhf
fi

echo "Cross Compiler: $CROSS_COMPILE"

# Create kernel build directory
echo "=== Downloading and preparing kernel ===" 
mkdir -p /tmp/kernel-headers
cd /tmp/kernel-headers

# Determine kernel source strategy based on version format
if [[ "$KERNEL_VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+ ]]; then
    # Looks like a specific kernel version (e.g., 5.10.103-v7l+, 6.12.34+rpt-rpi-2712)
    # Try to find the closest Raspberry Pi kernel tag or fall back to generic approach
    echo "Specific kernel version requested: $KERNEL_VERSION"
    
    # For specific versions, try to map to known working tags
    case "$KERNEL_VERSION" in
        5.10.103*)
            KERNEL_SOURCE_TAG="stable_20231123"
            echo "Mapping $KERNEL_VERSION to stable tag: $KERNEL_SOURCE_TAG"
            ;;
        6.12.34*)
            KERNEL_SOURCE_TAG="stable_20250127"
            echo "Mapping $KERNEL_VERSION to stable tag: $KERNEL_SOURCE_TAG"
            ;;
        6.1.70*)
            KERNEL_SOURCE_TAG="stable_20240529"
            echo "Mapping $KERNEL_VERSION to stable tag: $KERNEL_SOURCE_TAG"
            ;;
        *)
            # Fall back to a recent stable tag
            KERNEL_SOURCE_TAG="stable_20240529"
            echo "Unknown kernel version, using fallback tag: $KERNEL_SOURCE_TAG"
            ;;
    esac
else
    # Assume it's already a tag name
    KERNEL_SOURCE_TAG="$KERNEL_VERSION"
fi

# Download kernel source
if [ ! -f "$KERNEL_SOURCE_TAG.tar.gz" ]; then
    echo "Downloading kernel source tag $KERNEL_SOURCE_TAG..."
    wget https://github.com/raspberrypi/linux/archive/refs/tags/$KERNEL_SOURCE_TAG.tar.gz
fi

if [ ! -d "linux-$KERNEL_SOURCE_TAG" ]; then
    echo "Extracting kernel source..."
    tar -xzf $KERNEL_SOURCE_TAG.tar.gz
fi

cd linux-$KERNEL_SOURCE_TAG

# Configure kernel
echo "=== Configuring kernel ===" 
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE $KERNEL_CONFIG

# Disable problematic options for cross-compilation
if [ "$ARCH" = "arm" ]; then
    echo "Disabling function tracing for ARM to avoid -mrecord-mcount issues..."
    scripts/config --disable CONFIG_FUNCTION_TRACER
    scripts/config --disable CONFIG_FUNCTION_GRAPH_TRACER  
    scripts/config --disable CONFIG_DYNAMIC_FTRACE
    scripts/config --disable CONFIG_FTRACE
fi

# Apply configuration changes
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE oldconfig

# Prepare for module building
echo "=== Preparing kernel for module building ===" 
make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE modules_prepare

# Generate Module.symvers for symbol resolution
echo "=== Generating module symbols ==="
if [ ! -f Module.symvers ]; then
    touch Module.symvers
fi

export KERNEL_DIR=$(pwd)
echo "Kernel directory: $KERNEL_DIR"

# Build the modules
echo "=== Building Matrix Creator modules ===" 
cd /workspace/src

# Build kernel modules
echo "Building kernel modules..."
echo "Build command: make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE KDIR=\"$KERNEL_DIR\" -C \"$KERNEL_DIR\" M=$(pwd) modules"
echo "=== Starting module build with verbose output ==="
if ! make ARCH=$ARCH CROSS_COMPILE=$CROSS_COMPILE KBUILD_MODPOST_WARN=1 KDIR="$KERNEL_DIR" -C "$KERNEL_DIR" M=$(pwd) modules V=1 2>&1; then
    echo "ERROR: Module build failed!"
    echo "Checking for compilation errors..."
    echo "=== Current directory contents ==="
    ls -la
    echo "=== Object files ==="
    ls -la *.o 2>/dev/null || echo "No object files found"
    echo "=== Build dependency files ==="
    ls -la .*.d 2>/dev/null || echo "No dependency files found"
    exit 1
fi

echo "=== Module build completed successfully ==="
echo "=== Final object files ==="
ls -la *.o 2>/dev/null || echo "No object files found"
echo "=== Final kernel modules ==="
ls -la *.ko 2>/dev/null || echo "No kernel modules found"

# Build device tree overlay if present
if [ -f matrixio.dts ]; then
    echo "Building device tree overlay..."
    dtc -@ -I dts -O dtb -o matrixio.dtbo matrixio.dts
elif [ -f ../matrixio.dts ]; then
    echo "Building device tree overlay from parent directory..."
    dtc -@ -I dts -O dtb -o matrixio.dtbo ../matrixio.dts
fi

# Show build results
echo "=== Build Results ===" 
echo "Kernel modules (.ko files):"
ls -la *.ko 2>/dev/null || echo "No .ko files found"

echo "Device tree overlays (.dtbo files):"
ls -la *.dtbo 2>/dev/null || echo "No .dtbo files found"

echo "=== Build completed successfully! ===" 
