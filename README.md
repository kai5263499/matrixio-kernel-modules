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

Distributed under the same license as the original MATRIX Labs project.# Test CI fixes for kernel compatibility and versioning

## 🔄 Kernel Module Architecture: Old vs New

### Legacy Kernel Module Architecture (Pre-2024)

The original MATRIX kernel modules used a **direct hardware access** model:

```
[HAL Library] → [/dev/spidev0.0] → [SPI Bus] → [FPGA Hardware]
```

**Characteristics:**
- **Direct SPI Access**: HAL library communicated directly with `/dev/spidev0.0`
- **No Kernel Mediation**: SPI traffic went straight to FPGA without kernel module intervention
- **Raw FPGA Registers**: Device IDs and configuration read directly from FPGA memory
- **Expected Device IDs**: `0x05C344E8` (Creator) or `0x6032BAD2` (Voice)
- **Simple Device Tree**: Standard `spidev` enabled, minimal overlay

### Modern Kernel Module Architecture (Current)

The new kernel modules implement a **kernel-mediated access** model:

```
[HAL Library] → [/dev/matrixio_regmap] → [matrixio-core Module] → [SPI Bus] → [FPGA]
```

**Key Changes:**

#### 1. Device Tree Transformation
```dts
fragment@0 {
    target = <&spidev0>;
    __overlay__ { status = "disabled"; };  // ← Disables direct SPI access
};

fragment@2 {
    target = <&spi0>;
    __overlay__ {
        matrixio_core_0: matrixio_core@0 {   // ← Kernel module takes over SPI
            compatible = "matrixio-core";
            reg = <0>;                       // ← MATRIX device on spi0.0
        };
    };
};
```

#### 2. SPI Device Mapping
- **`spi0.0`**: `matrixio-core` (MATRIX device, kernel-controlled)
- **`spi0.1`**: `spidev` (generic SPI, available for direct access)
- **`/dev/spidev0.0`**: **Does not exist** (disabled by device tree)
- **`/dev/spidev0.1`**: Available but **wrong device** (not MATRIX hardware)

#### 3. Access Interfaces
- **`/dev/matrixio_regmap`**: Kernel module interface (ioctl: 1200/1201)
- **`/dev/matrixio_everloop`**: Direct LED control interface
- **ALSA devices**: `hw:2,0` for microphone array

#### 4. Device ID Changes
- **New Device ID**: `0x67452301` (returned via regmap interface)
- **Virtualized by Kernel**: May not reflect raw FPGA values
- **Consistent Interface**: Kernel module provides stable API regardless of FPGA firmware

### HAL Library Compatibility Requirements

#### Legacy HAL Behavior
```cpp
// Legacy: Try SPI first
if (bus_direct->Init()) {           // /dev/spidev0.0
    // Use SPI direct access
} else if (bus_kernel->Init()) {    // /dev/matrixio_regmap  
    // Fallback to kernel module
}
```

#### Modern HAL Behavior (Required)
```cpp
// Modern: Try kernel module first  
if (bus_kernel->Init()) {           // /dev/matrixio_regmap
    // Use kernel module interface (preferred)
} else if (bus_direct->Init()) {    // /dev/spidev0.0 (if available)
    // Fallback to direct SPI (legacy compatibility)
}
```

#### Device ID Support
```cpp
// Must recognize both legacy and new device IDs
const int kMatrixCreator = 0x05C344E8;      // Legacy
const int kMatrixVoice = 0x6032BAD2;        // Legacy  
const int kMatrixCreatorNew = 0x67452301;   // New kernel modules

if (matrix_name_ == kMatrixCreator || matrix_name_ == kMatrixCreatorNew) {
    matrix_leds_ = kMatrixCreatorNLeds;  // 35 LEDs
}
```

### Migration Impact

#### Applications Using HAL Library
- **✅ No changes required** if using updated HAL library
- **⚠️ Requires HAL update** to recognize new device ID and prioritize kernel interface

#### Applications Using Direct Interfaces  
- **✅ `/dev/matrixio_everloop`**: Continue to work (direct LED control)
- **✅ ALSA audio**: Continue to work (`arecord -D hw:2,0`)
- **❌ `/dev/spidev0.0`**: No longer available for direct MATRIX access
- **❌ Raw SPI access**: Must migrate to `/dev/matrixio_regmap` interface

### Benefits of New Architecture

1. **Stability**: Kernel module handles SPI communication edge cases
2. **Abstraction**: Consistent interface regardless of FPGA firmware version  
3. **Multi-device**: Kernel module can manage multiple MATRIX components
4. **Integration**: Better integration with Linux device model and ALSA
5. **Debugging**: Kernel module can provide better error handling and logging

### Testing Your System

```bash
# Check which architecture you're using
ls -la /dev/spidev0.* /dev/matrixio_*

# Legacy system will show:
# /dev/spidev0.0 (real device, not symlink)
# No /dev/matrixio_* devices

# Modern system will show:  
# /dev/spidev0.0 -> /dev/spidev0.1 (symlink, if created)
# /dev/spidev0.1 (generic SPI)
# /dev/matrixio_regmap (kernel module interface)
# /dev/matrixio_everloop (LED control)

# Check SPI device assignments
cat /sys/bus/spi/devices/spi0.0/modalias  # Should show: spi:matrixio-core
cat /sys/bus/spi/devices/spi0.1/modalias  # Should show: spi:spidev
```

This architectural change represents a fundamental shift from direct hardware access to a proper Linux kernel subsystem integration, providing better stability and functionality at the cost of requiring application updates.