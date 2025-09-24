#!/bin/bash

# Quick test for one configuration to verify fixes
set -e

echo "=== Quick Docker Build Test ==="
echo "Testing ARM32 build with stable_20240529..."

# Build Docker image
echo "Building Docker image..."
docker build -t matrixio-builder .

# Run single build test
echo "Running build test..."
docker run --rm \
    -v "$(pwd):/workspace" \
    -e ARCH="arm" \
    -e KERNEL_TAG="stable_20240529" \
    -e KERNEL_CONFIG="bcmrpi_defconfig" \
    matrixio-builder

# Check results
if ls src/*.ko 1> /dev/null 2>&1; then
    echo ""
    echo "🎉 SUCCESS! Kernel modules built:"
    ls -la src/*.ko
    echo ""
    echo "✅ Docker build system is working correctly!"
else
    echo "❌ FAILED: No kernel modules found"
    exit 1
fi
