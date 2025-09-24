# Matrix Creator Kernel Modules

This repository contains kernel modules designed for deployment on Raspberry Pi systems running Raspbian with Matrix Creator HATs.

## Overview

The Matrix Creator is a development board that provides sensors, wireless communications, and other functionality for IoT applications. These kernel modules enable low-level hardware access and control for Matrix Creator devices when attached to Raspberry Pi systems.

## Target Platforms

### Primary Target Systems

- **Legacy System**: Raspberry Pi 4 Model B Rev 1.4 (BCM2711)
  - Kernel: 5.10.103-v7l+
  - Architecture: ARM32 (armhf)
  - Use case: Proven stable environment with Matrix Creator

- **Latest System**: Raspberry Pi 5 (BCM2712) 
  - Kernel: 6.12.34+rpt-rpi-2712
  - Architecture: ARM64 (aarch64)
  - Use case: Latest hardware with modern kernel

### General Compatibility

- **Hardware**: Raspberry Pi with Matrix Creator HAT
- **OS**: Raspbian (Raspberry Pi OS)
- **Architecture**: ARM32 and ARM64 systems
- **Kernel Range**: 5.10.x to 6.12.x series

## Purpose

These kernel modules provide the necessary drivers and interfaces to interact with the Matrix Creator's hardware components, enabling applications to access sensors, wireless capabilities, and other features provided by the Matrix Creator ecosystem.

## Testing

### Comprehensive Testing Framework

The project includes a complete KUnit-based testing framework:

```bash
# Run all tests on all target platforms
./tests/run-tests-docker.sh

# Test specific platform
./tests/run-tests-docker.sh pi4-legacy    # Pi 4 with 5.10.103-v7l+
./tests/run-tests-docker.sh pi5-latest    # Pi 5 with 6.12.34-rpi-2712

# Test with verbose output
./tests/run-tests-docker.sh --verbose all

# Build modules only (no tests)
./tests/run-tests-docker.sh --build-only all
```

### Test Types

- **Unit Tests**: Core functionality testing for all modules (matrixio-core, matrixio-regmap, matrixio-env, matrixio-everloop)
- **Fuzzing Tests**: Robustness testing with random inputs for SPI interface, userspace interface, and device tree parsing
- **Mock Frameworks**: Comprehensive mocking for SPI, IIO, and platform devices

### Kernel Compatibility Testing

Use `docker/test-target-kernels.sh` to test cross-compilation across multiple kernel versions:

```bash
# Test only target kernel versions (Pi 4 legacy, Pi 5 latest)
./docker/test-target-kernels.sh --target-only

# Test only CI configurations  
./docker/test-target-kernels.sh --ci-only

# Quick single-kernel test
./docker/quick-test.sh
```

### Build Management

```bash
# Clean kernel module build artifacts
make clean

# Clean build test directories
make clean-tests

# Clean everything including test artifacts
make clean-all
```

### Available Scripts

- **`docker/build.sh`** - Core Docker build script (used internally)
- **`docker/quick-test.sh`** - Fast single-kernel compatibility test
- **`docker/test-target-kernels.sh`** - Comprehensive multi-kernel testing with target-specific options
- **`tests/run-tests-docker.sh`** - KUnit test runner with Docker containerization

## CI/CD Pipeline

The project uses a sequential test → build → package → publish workflow in GitHub Actions:

1. **Test Phase**: KUnit tests on all target platforms (pi4-legacy, pi5-latest, ci-standard)
2. **Build Phase**: Cross-compilation for ARM32/ARM64 (only if tests pass)
3. **Package Phase**: Debian package creation (only if build succeeds)
4. **Publish Phase**: Debian repository deployment to GitHub Pages

### Installation from Debian Repository

We provide split packaging for different target systems to ensure optimal compatibility:

#### Package Selection

Choose the appropriate package for your system:

- **matrixio-kernel-modules-buster**: For Raspbian Buster (Debian 10.13) ARM32 systems
  - Raspberry Pi 4 Model B Rev 1.4 (BCM2711) with 5.10.103-v7l+ kernel
  - Legacy Pi systems with 5.10.x kernel series

- **matrixio-kernel-modules-bookworm**: For Raspberry Pi OS Bookworm (Debian 12.12) ARM64 systems  
  - Raspberry Pi 5 with 6.12.34+rpt-rpi-2712 kernel
  - Updated Pi 4 systems with 6.1.70-rpi-v8 kernel

#### Automated Installation (Recommended)

```bash
curl -fsSL https://raw.githubusercontent.com/weswidner/matrixio-kernel-modules/master/scripts/install-repo.sh | bash
```

The automated installer detects your system and installs the appropriate package.

#### Manual Installation

```bash
# Add repository
echo "deb [trusted=yes] https://weswidner.github.io/matrixio-kernel-modules/ stable main" | sudo tee /etc/apt/sources.list.d/matrixio.list
sudo apt update

# Install the appropriate package for your system:

# For Pi 4 Legacy (Raspbian Buster):
sudo apt install matrixio-kernel-modules-buster

# For Pi 5 / Updated Pi 4 (Raspberry Pi OS Bookworm):
sudo apt install matrixio-kernel-modules-bookworm

# Enable the device tree overlay
echo "dtoverlay=matrixio" | sudo tee -a /boot/config.txt

# Reboot to activate
sudo reboot
```

### Semantic Versioning

Automated version management based on commit messages:

- `[version:major]` or `BREAKING CHANGE` → Major version bump
- `[version:minor]` or `feat:` → Minor version bump  
- `[version:patch]` or `fix:` → Patch version bump

See [VERSIONING.md](VERSIONING.md) for complete details on version management and release process.