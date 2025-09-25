#!/bin/bash

# Comprehensive kernel compatibility test script
# Tests against kernels that match target environments and CI configurations
# - Legacy: 5.10.103-v7l+ (Pi 4, BCM2711, ARM32) 
# - Latest: 6.12.34+rpt-rpi-2712 (Pi 5, BCM2712, ARM64)
# - CI configurations from GitHub Actions

set -e

# Show usage if --help
if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    echo "Usage: $0 [--target-only|--ci-only|--help]"
    echo ""
    echo "Options:"
    echo "  --target-only   Test only target kernel versions (Pi 4 legacy, Pi 5 latest)"
    echo "  --ci-only      Test only CI configurations (GitHub Actions)"
    echo "  --help         Show this help message"
    echo "  (no args)      Test all configurations"
    echo ""
    echo "Make targets:"
    echo "  make clean-tests    Clean build test artifacts"
    echo "  make clean-all      Clean everything including test artifacts"
    exit 0
fi

# Test mode selection
if [[ "$1" == "--target-only" ]]; then
    echo "=== Matrix Creator Target Kernel Test ==="
    echo "Testing against specific target kernel versions only"
    TEST_MODE="target"
elif [[ "$1" == "--ci-only" ]]; then
    echo "=== Matrix Creator CI Compatibility Test ==="
    echo "Testing against GitHub Actions CI configurations"
    TEST_MODE="ci"
else
    echo "=== Matrix Creator Comprehensive Kernel Test ==="
    echo "Testing against target kernels and CI configurations"
    TEST_MODE="all"
fi

echo ""

# Define test configurations
# Format: arch:kernel_tag:kernel_config:description:test_set
TARGET_CONFIGS=(
    # Target environments (matching user's specific systems)
    "arm:stable_20231123:bcmrpi_defconfig:Legacy Pi4 (5.10.x era):target"
    "arm64:rpi-6.12.y_20241206:bcm2712_defconfig:Latest Pi5 (6.12.x series):target"
)

CI_CONFIGS=(
    # CI configurations (from GitHub Actions)
    "arm:stable_20231123:bcmrpi_defconfig:CI bullseye:ci"
    "arm:stable_20240529:bcmrpi_defconfig:CI bookworm ARM32:ci" 
    "arm64:stable_20250127:bcm2711_defconfig:CI bookworm ARM64:ci"
)

# Select configurations based on test mode
CONFIGS=()
case $TEST_MODE in
    "target")
        CONFIGS=("${TARGET_CONFIGS[@]}")
        ;;
    "ci")
        CONFIGS=("${CI_CONFIGS[@]}")
        ;;
    "all")
        CONFIGS=("${TARGET_CONFIGS[@]}" "${CI_CONFIGS[@]}")
        ;;
esac

# Function to build for a specific configuration
build_config() {
    local arch=$1
    local kernel_tag=$2
    local kernel_config=$3
    local description=$4
    
    echo ""
    echo "=== Testing: $description ==="
    echo "    Architecture: $arch"
    echo "    Kernel tag: $kernel_tag"
    echo "    Config: $kernel_config"
    echo ""
    
    # Build Docker image (always rebuild to ensure latest changes)
    echo "Building Docker image..."
    docker build -t matrixio-builder .
    
    # Create a clean build directory for this test
    BUILD_DIR="build-test-$(echo $kernel_tag | tr '/' '-')-$arch"
    mkdir -p "$BUILD_DIR"
    
    # Run the build
    echo "Running build in container..."
    if docker run --rm \
        --platform linux/amd64 \
        -v "$(pwd):/workspace" \
        -e ARCH="$arch" \
        -e KERNEL_TAG="$kernel_tag" \
        -e KERNEL_CONFIG="$kernel_config" \
        matrixio-builder 2>&1 | tee "$BUILD_DIR/build.log"; then
        
        # Check results
        if ls src/*.ko 1> /dev/null 2>&1; then
            echo "✅ SUCCESS: $description"
            echo "   Kernel modules built:"
            ls -la src/*.ko | while read line; do echo "   $line"; done
            
            # Copy artifacts to build directory
            cp src/*.ko "$BUILD_DIR/" 2>/dev/null || true
            cp src/*.dtbo "$BUILD_DIR/" 2>/dev/null || true
            
            echo "   Artifacts saved to: $BUILD_DIR/"
            return 0
        else
            echo "❌ FAILED: $description - No kernel modules found"
            return 1
        fi
    else
        echo "❌ FAILED: $description - Build failed"
        return 1
    fi
}

# Function to clean up build artifacts
cleanup_build() {
    echo "Cleaning up build artifacts..."
    rm -f src/*.ko src/*.o src/*.mod src/*.mod.c src/*.dtbo 2>/dev/null || true
    find src -name ".*cmd" -delete 2>/dev/null || true
    find src -name ".*d" -delete 2>/dev/null || true
}

# Main execution
echo "Starting target kernel compatibility tests..."
echo ""

TOTAL=0
PASSED=0
FAILED=0

for config in "${CONFIGS[@]}"; do
    IFS=':' read -ra PARTS <<< "$config"
    arch="${PARTS[0]}"
    kernel_tag="${PARTS[1]}"
    kernel_config="${PARTS[2]}"
    description="${PARTS[3]}"
    test_set="${PARTS[4]}"
    
    TOTAL=$((TOTAL + 1))
    
    if build_config "$arch" "$kernel_tag" "$kernel_config" "$description"; then
        PASSED=$((PASSED + 1))
    else
        FAILED=$((FAILED + 1))
    fi
    
    # Clean up after each test
    cleanup_build
    
    echo ""
done

# Summary
echo "=============================================="
echo "=== TARGET KERNEL COMPATIBILITY SUMMARY ==="
echo "=============================================="
echo "Total tests: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "🎉 ALL TARGET KERNELS PASS!"
    echo "✅ Your kernel modules are compatible with both:"
    echo "   - Legacy Pi 4 (5.10.x series) environments"
    echo "   - Latest Pi 5 (6.12.x series) environments"
    echo ""
    echo "Build artifacts are saved in build-test-* directories"
else
    echo "💥 SOME TARGET KERNELS FAILED!"
    echo "❌ Compatibility issues detected"
    echo ""
    echo "Check the build logs in build-test-* directories for details"
    exit 1
fi

echo "=============================================="