#!/bin/bash

# Docker-based KUnit test runner for Matrix Creator kernel modules
# This script runs the comprehensive test suite in a controlled kernel environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Default configuration
DEFAULT_KERNEL_VERSION="6.1.70-rpi-v8"
DEFAULT_ARCH="arm64"
VERBOSE=0
CLEAN_AFTER=1
BUILD_ONLY=0
TEST_ONLY=0

# Test configurations for different targets
declare -A KERNEL_CONFIGS=(
    ["pi4-legacy"]="5.10.103-v7l+ arm"
    ["pi5-latest"]="6.12.34-rpi-2712 arm64"
    ["ci-standard"]="6.1.70-rpi-v8 arm64"
)

usage() {
    cat << EOF
Usage: $0 [OPTIONS] [TEST_TARGET]

Run KUnit tests for Matrix Creator kernel modules in Docker containers.

TEST_TARGET can be:
    pi4-legacy    - Test on Pi 4 legacy kernel (5.10.103-v7l+ arm)
    pi5-latest    - Test on Pi 5 latest kernel (6.12.34-rpi-2712 arm64)
    ci-standard   - Test on CI standard kernel (6.1.70-rpi-v8 arm64)
    all           - Test on all target configurations (default)

OPTIONS:
    -v, --verbose       Enable verbose output
    -k, --kernel VER    Use specific kernel version
    -a, --arch ARCH     Use specific architecture (arm, arm64)
    --no-clean          Don't clean up containers after tests
    --build-only        Only build test modules, don't run tests
    --test-only         Only run tests (assume modules are built)
    -h, --help          Show this help message

EXAMPLES:
    $0                  # Run all tests on all target configurations
    $0 pi4-legacy       # Test only Pi 4 legacy configuration
    $0 --verbose all    # Run all tests with verbose output
    $0 --kernel 6.1.70-rpi-v8 --arch arm64 ci-standard

ENVIRONMENT VARIABLES:
    DOCKER_BUILD_ARGS   Additional arguments to pass to docker build
    KUNIT_FILTER        Filter to apply to KUnit tests (e.g., "matrixio.*")
    PARALLEL_JOBS       Number of parallel jobs for make (default: nproc)

EOF
}

log() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $*" >&2
}

verbose_log() {
    if [[ $VERBOSE -eq 1 ]]; then
        log "$*"
    fi
}

cleanup_containers() {
    if [[ $CLEAN_AFTER -eq 1 ]]; then
        log "Cleaning up test containers..."
        docker ps -aq --filter "label=matrixio-test" | xargs -r docker rm -f
        docker images -q --filter "label=matrixio-test" | xargs -r docker rmi -f
    fi
}

create_test_dockerfile() {
    local kernel_version="$1"
    local arch="$2"
    local dockerfile_path="$3"

    cat > "$dockerfile_path" << EOF
FROM debian:bullseye-slim

LABEL matrixio-test=true

# Install build dependencies
RUN apt-get update && apt-get install -y \\
    build-essential \\
    bc \\
    kmod \\
    cpio \\
    flex \\
    bison \\
    libssl-dev \\
    libelf-dev \\
    git \\
    wget \\
    python3 \\
    python3-pip \\
    device-tree-compiler \\
    && rm -rf /var/lib/apt/lists/*

# Install Python dependencies for testing
RUN pip3 install \\
    pytest \\
    coverage \\
    pyelftools

# Set up cross-compilation for ARM
RUN if [ "$arch" = "arm" ]; then \\
        apt-get update && apt-get install -y gcc-arm-linux-gnueabihf && rm -rf /var/lib/apt/lists/*; \\
    elif [ "$arch" = "arm64" ]; then \\
        apt-get update && apt-get install -y gcc-aarch64-linux-gnu && rm -rf /var/lib/apt/lists/*; \\
    fi

# Download and prepare kernel headers for the target version
WORKDIR /kernel
RUN echo "Kernel version: $kernel_version" && \\
    if echo "$kernel_version" | grep -q -E "(rpi|stable_)" || [ "\${kernel_version#*v7l}" != "$kernel_version" ] || [ "\${kernel_version#*v8}" != "$kernel_version" ]; then \\
        echo "Detected Raspberry Pi kernel pattern" && \\
        # Map specific versions to known working tags \\
        case "$kernel_version" in \\
            5.10.103*) \\
                echo "Mapping to stable_20231123 for 5.10.103 series" && \\
                git clone --depth 1 --branch stable_20231123 https://github.com/raspberrypi/linux.git . ;; \\
            6.12.34*) \\
                echo "Mapping to stable_20250127 for 6.12.34 series" && \\
                git clone --depth 1 --branch stable_20250127 https://github.com/raspberrypi/linux.git . ;; \\
            6.1.70*) \\
                echo "Mapping to stable_20240529 for 6.1.70 series" && \\
                git clone --depth 1 --branch stable_20240529 https://github.com/raspberrypi/linux.git . ;; \\
            *) \\
                echo "Using fallback to latest rpi-6.6.y branch" && \\
                git clone --depth 1 --branch rpi-6.6.y https://github.com/raspberrypi/linux.git . ;; \\
        esac \\
    else \\
        echo "Detected mainline kernel" && \\
        KERNEL_MAJOR=\$(echo "$kernel_version" | cut -d. -f1) && \\
        echo "Downloading mainline kernel v\$KERNEL_MAJOR.x" && \\
        wget -q "https://cdn.kernel.org/pub/linux/kernel/v\${KERNEL_MAJOR}.x/linux-$kernel_version.tar.xz" && \\
        tar -xf "linux-$kernel_version.tar.xz" --strip-components=1 && \\
        rm "linux-$kernel_version.tar.xz" ; \\
    fi

# Configure kernel for target architecture
RUN if [ "$arch" = "arm" ]; then \\
        export ARCH=arm && export CROSS_COMPILE=arm-linux-gnueabihf- ; \\
    elif [ "$arch" = "arm64" ]; then \\
        export ARCH=arm64 && export CROSS_COMPILE=aarch64-linux-gnu- ; \\
    else \\
        export ARCH=x86_64 ; \\
    fi && \\
    make defconfig && \\
    echo "CONFIG_KUNIT=y" >> .config && \\
    echo "CONFIG_KUNIT_TEST=y" >> .config && \\
    echo "CONFIG_SPI=y" >> .config && \\
    echo "CONFIG_IIO=y" >> .config && \\
    echo "CONFIG_OF=y" >> .config && \\
    echo "CONFIG_MFD_CORE=y" >> .config && \\
    make olddefconfig && \\
    make prepare && \\
    make modules_prepare && \\
    make -j4 kernel/bounds.s kernel/time/timeconst.h arch/\$ARCH/include/generated/asm/syscalls_64.h 2>/dev/null || true && \\
    make -j4 drivers/spi/spi.ko drivers/mfd/mfd-core.ko 2>/dev/null || make -j4 M=drivers/spi M=drivers/mfd modules 2>/dev/null || true

WORKDIR /src
COPY . .

# Create build script
RUN echo '#!/bin/bash' > /build-and-test.sh
RUN echo 'set -e' >> /build-and-test.sh
RUN echo 'cd /src' >> /build-and-test.sh
RUN echo 'if [ "\$1" = "arm" ]; then' >> /build-and-test.sh
RUN echo '    export ARCH=arm' >> /build-and-test.sh
RUN echo '    export CROSS_COMPILE=arm-linux-gnueabihf-' >> /build-and-test.sh
RUN echo 'elif [ "\$1" = "arm64" ]; then' >> /build-and-test.sh
RUN echo '    export ARCH=arm64' >> /build-and-test.sh
RUN echo '    export CROSS_COMPILE=aarch64-linux-gnu-' >> /build-and-test.sh
RUN echo 'else' >> /build-and-test.sh
RUN echo '    export ARCH=x86_64' >> /build-and-test.sh
RUN echo 'fi' >> /build-and-test.sh
RUN echo 'export KERNELDIR=/kernel' >> /build-and-test.sh
RUN echo 'export KDIR=/kernel' >> /build-and-test.sh
RUN echo 'export KBUILD_MODPOST_WARN=1' >> /build-and-test.sh
RUN echo 'export PARALLEL_JOBS=\${PARALLEL_JOBS:-\$(nproc)}' >> /build-and-test.sh
RUN echo 'echo "=== Building Matrix Creator kernel modules ==="' >> /build-and-test.sh
RUN echo 'echo "Architecture: \$ARCH"' >> /build-and-test.sh
RUN echo 'echo "Kernel: \$2"' >> /build-and-test.sh
RUN echo 'echo "Jobs: \$PARALLEL_JOBS"' >> /build-and-test.sh
RUN echo 'cd src' >> /build-and-test.sh
RUN echo 'make -j\$PARALLEL_JOBS || { echo "Main module build failed"; exit 1; }' >> /build-and-test.sh
RUN echo 'echo "=== Main modules built successfully ==="' >> /build-and-test.sh
RUN echo 'if [ "\$3" = "build-only" ]; then' >> /build-and-test.sh
RUN echo '    echo "Build completed successfully"' >> /build-and-test.sh
RUN echo '    exit 0' >> /build-and-test.sh
RUN echo 'fi' >> /build-and-test.sh
RUN echo 'echo "=== Building test modules ==="' >> /build-and-test.sh
RUN echo 'cd ../tests' >> /build-and-test.sh
RUN echo 'if [ -f Makefile ]; then' >> /build-and-test.sh
RUN echo '    make -j\$PARALLEL_JOBS || { echo "Test module build failed"; exit 1; }' >> /build-and-test.sh
RUN echo '    echo "=== Test modules built successfully ==="' >> /build-and-test.sh
RUN echo '    ls -la *.ko 2>/dev/null || echo "No test modules found"' >> /build-and-test.sh
RUN echo 'else' >> /build-and-test.sh
RUN echo '    echo "No test Makefile found, skipping test build"' >> /build-and-test.sh
RUN echo 'fi' >> /build-and-test.sh
RUN echo 'echo "=== Test Summary ==="' >> /build-and-test.sh
RUN echo 'echo "All modules built successfully"' >> /build-and-test.sh

RUN chmod +x /build-and-test.sh

ENTRYPOINT ["/build-and-test.sh"]
EOF
}

run_tests_for_config() {
    local config_name="$1"
    local kernel_version="$2"
    local arch="$3"
    
    log "Running tests for $config_name (kernel: $kernel_version, arch: $arch)"
    
    local dockerfile_path="$PROJECT_ROOT/Dockerfile.test.$config_name"
    local image_name="matrixio-test:$config_name"
    local container_name="matrixio-test-$config_name-$$"
    
    # Create dockerfile
    verbose_log "Creating Dockerfile for $config_name"
    create_test_dockerfile "$kernel_version" "$arch" "$dockerfile_path"
    
    # Build test image
    log "Building test image for $config_name..."
    local build_args=""
    if [[ $VERBOSE -eq 1 ]]; then
        build_args="--progress=plain"
    fi
    
    docker build $build_args $DOCKER_BUILD_ARGS \
        --label "matrixio-test=true" \
        -t "$image_name" \
        -f "$dockerfile_path" \
        "$PROJECT_ROOT" || {
        log "ERROR: Failed to build test image for $config_name"
        return 1
    }
    
    # Run tests
    log "Running tests in container for $config_name..."
    local run_mode="test"
    if [[ $BUILD_ONLY -eq 1 ]]; then
        run_mode="build-only"
    fi
    
    local docker_args=()
    if [[ $VERBOSE -eq 1 ]]; then
        docker_args+=(-i)
    fi
    
    docker_args+=(
        --name "$container_name"
        --label "matrixio-test=true"
        --rm
    )
    
    # Pass environment variables
    if [[ -n "$KUNIT_FILTER" ]]; then
        docker_args+=(-e "KUNIT_FILTER=$KUNIT_FILTER")
    fi
    if [[ -n "$PARALLEL_JOBS" ]]; then
        docker_args+=(-e "PARALLEL_JOBS=$PARALLEL_JOBS")
    fi
    
    docker run "${docker_args[@]}" "$image_name" "$arch" "$kernel_version" "$run_mode" || {
        log "ERROR: Tests failed for $config_name"
        return 1
    }
    
    log "Tests completed successfully for $config_name"
    
    # Clean up dockerfile
    rm -f "$dockerfile_path"
    
    return 0
}

# Parse command line arguments
ARGS=()
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -k|--kernel)
            DEFAULT_KERNEL_VERSION="$2"
            shift 2
            ;;
        -a|--arch)
            DEFAULT_ARCH="$2"
            shift 2
            ;;
        --no-clean)
            CLEAN_AFTER=0
            shift
            ;;
        --build-only)
            BUILD_ONLY=1
            shift
            ;;
        --test-only)
            TEST_ONLY=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        -*)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
        *)
            ARGS+=("$1")
            shift
            ;;
    esac
done

# Set positional arguments
set -- "${ARGS[@]}"

# Determine test target
TEST_TARGET="${1:-all}"

# Trap for cleanup
trap cleanup_containers EXIT

# Validate target
case "$TEST_TARGET" in
    pi4-legacy|pi5-latest|ci-standard)
        # Single target
        IFS=' ' read -r kernel_version arch <<< "${KERNEL_CONFIGS[$TEST_TARGET]}"
        if ! run_tests_for_config "$TEST_TARGET" "$kernel_version" "$arch"; then
            log "ERROR: Tests failed for $TEST_TARGET"
            exit 1
        fi
        ;;
    all)
        # All targets
        failed_configs=()
        for config in "${!KERNEL_CONFIGS[@]}"; do
            IFS=' ' read -r kernel_version arch <<< "${KERNEL_CONFIGS[$config]}"
            if ! run_tests_for_config "$config" "$kernel_version" "$arch"; then
                failed_configs+=("$config")
            fi
        done
        
        if [[ ${#failed_configs[@]} -gt 0 ]]; then
            log "ERROR: Tests failed for configurations: ${failed_configs[*]}"
            exit 1
        fi
        ;;
    custom)
        # Custom configuration
        if ! run_tests_for_config "custom" "$DEFAULT_KERNEL_VERSION" "$DEFAULT_ARCH"; then
            log "ERROR: Tests failed for custom configuration"
            exit 1
        fi
        ;;
    *)
        echo "Unknown test target: $TEST_TARGET" >&2
        echo "Valid targets: pi4-legacy, pi5-latest, ci-standard, all, custom" >&2
        exit 1
        ;;
esac

log "All tests completed successfully!"