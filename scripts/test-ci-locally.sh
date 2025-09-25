#!/bin/bash

# Test CI pipeline locally to exactly match GitHub Actions environment
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0Check wm' # No Color

log() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

header() {
    echo -e "${BLUE}=== $* ===${NC}"
}

# Test configurations matching GitHub Actions
TEST_CONFIGS=(
    "pi4-legacy:5.10.103-v7l+:arm"
    "pi5-latest:6.12.34+rpt-rpi-2712:arm64"
    "ci-standard:6.1.70-rpi-v8:arm64"
)

test_kernel_download() {
    local target="$1"
    local kernel_version="$2"
    local arch="$3"
    
    header "Testing kernel download for $target ($kernel_version, $arch)"
    
    # Create a minimal test Dockerfile to debug kernel download
    cat > "$PROJECT_ROOT/test-kernel-${target}.dockerfile" << EOF
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install minimal dependencies
RUN apt-get update && apt-get install -y \\
    git \\
    wget \\
    curl \\
    ca-certificates \\
    && rm -rf /var/lib/apt/lists/*

WORKDIR /kernel

# Test the exact kernel download logic from the test runner
RUN echo "Testing kernel version: $kernel_version" && \\
    echo "Architecture: $arch" && \\
    if echo "$kernel_version" | grep -q -E "(rpi|stable_)" || [ "\${kernel_version#*v7l}" != "$kernel_version" ] || [ "\${kernel_version#*v8}" != "$kernel_version" ]; then \\
        echo "Detected Raspberry Pi kernel pattern" && \\
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
    fi && \\
    echo "Kernel source download completed" && \\
    ls -la | head -10

ENV kernel_version=$kernel_version
EOF

    log "Building test container for $target..."
    cd "$PROJECT_ROOT"
    if docker build -t test-kernel-${target} -f test-kernel-${target}.dockerfile . 2>&1; then
        log "✅ Kernel download test passed for $target"
        return 0
    else
        error "❌ Kernel download test failed for $target"
        return 1
    fi
}

test_full_docker_build() {
    local target="$1"
    local kernel_version="$2"
    local arch="$3"
    
    header "Testing full Docker build for $target"
    
    cd "$PROJECT_ROOT/tests"
    
    # Set environment variables exactly as GitHub Actions would
    export GITHUB_WORKSPACE="$PROJECT_ROOT"
    export RUNNER_TEMP="/tmp"
    
    log "Running Docker test runner for $target..."
    if timeout 600 bash ./run-tests-docker.sh --verbose --build-only "$target" 2>&1; then
        log "✅ Full Docker build test passed for $target"
        return 0
    else
        error "❌ Full Docker build test failed for $target"
        return 1
    fi
}

# Test if we can run ARM containers on this system
test_arm_emulation() {
    header "Testing ARM container emulation"
    
    if docker run --rm --platform linux/arm64 ubuntu:22.04 uname -a 2>/dev/null; then
        log "✅ ARM64 emulation available"
        ARM64_AVAILABLE=1
    else
        warn "❌ ARM64 emulation not available"
        ARM64_AVAILABLE=0
    fi
    
    if docker run --rm --platform linux/arm/v7 ubuntu:22.04 uname -a 2>/dev/null; then
        log "✅ ARM32 emulation available"
        ARM32_AVAILABLE=1
    else
        warn "❌ ARM32 emulation not available"
        ARM32_AVAILABLE=0
    fi
}

check_github_actions_requirements() {
    header "Checking GitHub Actions Requirements"
    
    # Check if we can use ARM runners
    log "GitHub Actions runner options:"
    log "1. Standard (x86_64): ubuntu-latest, ubuntu-22.04, ubuntu-20.04"
    log "2. ARM runners: Available via third-party (Actuated, BuildJet) or self-hosted"
    log "3. Cross-compilation: Build on x86_64 with cross-compile tools"
    
    if [[ "$ARM64_AVAILABLE" == "1" && "$ARM32_AVAILABLE" == "1" ]]; then
        log "✅ This system can emulate both ARM architectures"
        log "✅ Cross-compilation approach should work in GitHub Actions"
    else
        warn "⚠️  Limited ARM emulation - consider using ARM runners for better compatibility"
    fi
}

main() {
    header "CI Local Testing - Matrix Creator Kernel Modules"
    
    log "Testing environment:"
    log "  - Docker version: $(docker --version)"
    log "  - Host architecture: $(uname -m)"
    log "  - Project root: $PROJECT_ROOT"
    
    # Test ARM emulation capabilities
    test_arm_emulation
    
    # Check GitHub Actions requirements
    check_github_actions_requirements
    
    # Test kernel downloads first (faster feedback)
    log "Phase 1: Testing kernel source downloads..."
    failed_downloads=()
    for config in "${TEST_CONFIGS[@]}"; do
        IFS=':' read -r target kernel_version arch <<< "$config"
        if ! test_kernel_download "$target" "$kernel_version" "$arch"; then
            failed_downloads+=("$target")
        fi
    done
    
    if [[ ${#failed_downloads[@]} -gt 0 ]]; then
        error "Kernel download failures: ${failed_downloads[*]}"
        log "Continuing with full build tests..."
    fi
    
    # Test full Docker builds
    log "Phase 2: Testing full Docker builds..."
    failed_builds=()
    for config in "${TEST_CONFIGS[@]}"; do
        IFS=':' read -r target kernel_version arch <<< "$config"
        if ! test_full_docker_build "$target" "$kernel_version" "$arch"; then
            failed_builds+=("$target")
        fi
    done
    
    # Summary
    header "Test Results Summary"
    if [[ ${#failed_downloads[@]} -eq 0 && ${#failed_builds[@]} -eq 0 ]]; then
        log "🎉 All tests passed! CI should work correctly."
    else
        error "❌ Some tests failed:"
        [[ ${#failed_downloads[@]} -gt 0 ]] && error "  Kernel downloads: ${failed_downloads[*]}"
        [[ ${#failed_builds[@]} -gt 0 ]] && error "  Docker builds: ${failed_builds[*]}"
        exit 1
    fi
    
    # Cleanup
    log "Cleaning up test containers..."
    docker images | grep "test-kernel-" | awk '{print $3}' | xargs -r docker rmi -f
    rm -f "$PROJECT_ROOT"/test-kernel-*.dockerfile
}

# Handle script arguments
case "${1:-}" in
    --kernel-only)
        # Test only kernel downloads
        for config in "${TEST_CONFIGS[@]}"; do
            IFS=':' read -r target kernel_version arch <<< "$config"
            test_kernel_download "$target" "$kernel_version" "$arch"
        done
        ;;
    --build-only)
        # Test only Docker builds
        for config in "${TEST_CONFIGS[@]}"; do
            IFS=':' read -r target kernel_version arch <<< "$config"
            test_full_docker_build "$target" "$kernel_version" "$arch"
        done
        ;;
    --help)
        echo "Usage: $0 [OPTIONS]"
        echo ""
        echo "Test CI pipeline locally to match GitHub Actions environment"
        echo ""
        echo "OPTIONS:"
        echo "  --kernel-only   Test only kernel source downloads"
        echo "  --build-only    Test only Docker builds"
        echo "  --help          Show this help"
        echo ""
        echo "Default: Run all tests"
        ;;
    "")
        main
        ;;
    *)
        error "Unknown option: $1"
        echo "Use --help for usage information"
        exit 1
        ;;
esac