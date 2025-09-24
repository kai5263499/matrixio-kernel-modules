# MATRIX Creator Kernel Modules

Enhanced kernel drivers for MATRIX Creator and MATRIX Voice HATs with comprehensive cross-kernel compatibility and automated packaging.

## ✨ What's New

This fork includes major enhancements over the upstream version:

- **Cross-kernel compatibility** - Works with kernels 5.10.x through 6.12.x
- **Automated debian packaging** - Split packages for different target systems  
- **Comprehensive testing** - Docker-based testing across multiple kernel versions
- **GitHub Actions CI/CD** - Automated builds, tests, and package publishing
- **Live debian repository** - Install via apt from GitHub Pages

## 🎯 Supported Systems

| Target | Kernel | Architecture | Package |
|--------|--------|--------------|---------|
| **Pi 5 Latest** | 6.12.47+rpt-rpi-2712 | ARM64 | matrixio-kernel-modules-bookworm |
| **Pi 4 Current** | 6.1.70-rpi-v8 | ARM64 | matrixio-kernel-modules-bookworm |
| **Pi 4 Legacy** | 5.10.103-v7l+ | ARM32 | matrixio-kernel-modules-buster |

## 🚀 Quick Installation (Recommended)

### Option 1: Automated Package Installation

```bash
# Add repository
echo "deb [trusted=yes] https://kai5263499.github.io/matrixio-kernel-modules/ stable main" | sudo tee /etc/apt/sources.list.d/matrixio.list

# Update and install
sudo apt update
sudo apt install matrixio-kernel-modules

# Reboot to load modules
sudo reboot
```

### Option 2: Manual Package Download

Browse and download packages directly from: https://kai5263499.github.io/matrixio-kernel-modules/pool/main/

## 🔧 Development Installation

### Prerequisites

```bash
# Install build dependencies
sudo apt update
sudo apt install -y \
  raspberrypi-kernel-headers \
  build-essential \
  git \
  device-tree-compiler \
  dkms

# Install creator init (if needed)
sudo apt install matrixio-creator-init
```

### Build from Source

```bash
# Clone repository
git clone https://github.com/kai5263499/matrixio-kernel-modules.git
cd matrixio-kernel-modules

# Build and install
cd src
make && sudo make install

# Setup device tree overlay
echo "dtoverlay=matrixio" | sudo tee -a /boot/config.txt

# Install module configuration
sudo cp ../misc/matrixio.conf /etc/modules-load.d/
sudo cp ../misc/asound.conf /etc/

# Reboot to activate
sudo reboot
```

## 🧪 Testing & Development

### Local Testing

```bash
# Quick compatibility test
./scripts/quick-test.sh

# Full test suite (requires Docker)
cd tests
./run-tests-docker.sh --build-only ci-standard
```

### Supported Test Targets

- `pi4-legacy` - Raspberry Pi 4 with 5.10.103-v7l+ kernel (ARM32)
- `pi5-latest` - Raspberry Pi 5 with 6.12.47+rpt-rpi-2712 kernel (ARM64)  
- `ci-standard` - CI environment with 6.1.70-rpi-v8 kernel (ARM64)

## 📦 Package Architecture

### Split Debian Packaging

- **debian-bookworm/** - Modern ARM64 systems (Pi 5, updated Pi 4)
- **debian-buster/** - Legacy ARM32 systems (Pi 4 legacy)

### Automated Versioning

Commits trigger automatic version increments:
- `[version:major]` or `BREAKING CHANGE` → major version bump
- `[version:minor]` or `feat:` → minor version bump  
- `[version:patch]` or `fix:` → patch version bump
- Default → automatic patch increment

## 🏗️ CI/CD Pipeline

The repository includes a comprehensive GitHub Actions pipeline:

1. **Testing** - Cross-compilation and KUnit tests across target kernels
2. **Building** - Kernel module compilation for each target
3. **Packaging** - Debian package creation with DKMS integration
4. **Publishing** - Automated debian repository deployment to GitHub Pages

## 🔍 Kernel Compatibility Layer

The enhanced compatibility layer (`src/matrixio-compat.h`) handles:

- `class_create()` API changes (kernel 6.4+)
- `dev_uevent` function signature changes  
- Platform driver `remove` function signatures
- Cross-version type compatibility

## 📋 Module Information

| Module | Description | Location |
|--------|-------------|----------|
| matrixio-core | Core MFD functionality | /kernel/drivers/mfd |
| matrixio-codec | Audio codec support | /kernel/sound/soc/codecs |
| matrixio-mic | Microphone interface | /kernel/sound/soc/codecs |
| matrixio-playback | Audio playback | /kernel/sound/soc/codecs |
| matrixio-env | Environmental sensors | /kernel/drivers/mfd |
| matrixio-imu | IMU sensors | /kernel/drivers/mfd |
| matrixio-everloop | LED ring control | /kernel/drivers/mfd |
| matrixio-gpio | GPIO interface | /kernel/drivers/mfd |
| matrixio-uart | UART interface | /kernel/drivers/mfd |
| matrixio-regmap | Register map interface | /kernel/drivers/mfd |

## 🐛 Troubleshooting

### Common Issues

**Module fails to load:**
```bash
# Check kernel version compatibility
uname -r

# Verify DKMS status  
sudo dkms status matrixio-kernel-modules

# Rebuild if needed
sudo dkms remove matrixio-kernel-modules/$(cat VERSION) --all
sudo dkms install matrixio-kernel-modules/$(cat VERSION)
```

**Device tree overlay not found:**
```bash
# Verify overlay exists
ls -la /boot/overlays/matrixio.dtbo

# Check config.txt
grep matrixio /boot/config.txt
```

## 📚 Additional Resources

- **Original upstream:** https://github.com/matrix-io/matrixio-kernel-modules
- **Live repository:** https://kai5263499.github.io/matrixio-kernel-modules/
- **CI/CD status:** https://github.com/kai5263499/matrixio-kernel-modules/actions

## 📄 License

Distributed under the same license as the original MATRIX Labs project.