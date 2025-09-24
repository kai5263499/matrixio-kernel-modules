#!/bin/bash

# Matrix Creator Debian Repository Installation Script
# This script adds the Matrix Creator repository to your Raspberry Pi system

set -e

REPO_URL="https://weswidner.github.io/matrixio-kernel-modules"
REPO_NAME="matrixio"
LIST_FILE="/etc/apt/sources.list.d/${REPO_NAME}.list"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

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

# Check if running as root
check_root() {
    if [[ $EUID -eq 0 ]]; then
        error "This script should not be run as root. Run as a regular user with sudo privileges."
        exit 1
    fi
}

# Check if running on Raspberry Pi
check_raspberry_pi() {
    if [[ ! -f /proc/device-tree/model ]] || ! grep -q "Raspberry Pi" /proc/device-tree/model 2>/dev/null; then
        warn "This doesn't appear to be a Raspberry Pi system."
        read -p "Continue anyway? (y/N): " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            exit 1
        fi
    else
        local model=$(tr -d '\0' < /proc/device-tree/model)
        log "Detected: $model"
    fi
}

# Detect target platform and appropriate package
detect_platform() {
    local kernel_version=$(uname -r)
    local arch=$(uname -m)
    local debian_version=""
    
    log "Kernel version: $kernel_version"
    log "Architecture: $arch"
    
    # Detect Debian version if available
    if [[ -f /etc/debian_version ]]; then
        debian_version=$(cat /etc/debian_version)
        log "Debian version: $debian_version"
    fi
    
    # Determine which package to install based on system characteristics
    if [[ "$kernel_version" =~ 5\.10\. ]] && [[ "$arch" == "armv7l" ]]; then
        log "Platform: Pi 4 Legacy (5.10.x kernel, ARM32)"
        PACKAGE_NAME="matrixio-kernel-modules-buster"
        PLATFORM_DESC="Raspbian Buster (Debian 10.13) ARM32"
    elif [[ "$kernel_version" =~ 6\.[0-9]+\. ]] && [[ "$arch" == "aarch64" ]]; then
        log "Platform: Pi 5 / Updated Pi 4 (6.x kernel, ARM64)"
        PACKAGE_NAME="matrixio-kernel-modules-bookworm"
        PLATFORM_DESC="Raspberry Pi OS Bookworm (Debian 12.12) ARM64"
    elif [[ "$arch" == "aarch64" ]]; then
        # Assume newer system for ARM64
        log "Platform: ARM64 system (assuming Bookworm)"
        PACKAGE_NAME="matrixio-kernel-modules-bookworm" 
        PLATFORM_DESC="Raspberry Pi OS Bookworm (Debian 12.12) ARM64"
    elif [[ "$arch" == "armv7l" ]]; then
        # Assume legacy system for ARM32
        log "Platform: ARM32 system (assuming Buster)"
        PACKAGE_NAME="matrixio-kernel-modules-buster"
        PLATFORM_DESC="Raspbian Buster (Debian 10.13) ARM32"
    else
        warn "Platform not specifically targeted"
        log "Architecture '$arch' and kernel '$kernel_version' don't match target profiles"
        # Default to Bookworm package
        PACKAGE_NAME="matrixio-kernel-modules-bookworm"
        PLATFORM_DESC="Generic (defaulting to Bookworm package)"
    fi
    
    log "Selected package: $PACKAGE_NAME"
    log "Target platform: $PLATFORM_DESC"
}

# Add repository
add_repository() {
    header "Adding Matrix Creator Repository"
    
    # Remove existing repository if present
    if [[ -f "$LIST_FILE" ]]; then
        warn "Existing repository configuration found. Removing..."
        sudo rm -f "$LIST_FILE"
    fi
    
    # Add new repository
    log "Adding repository: $REPO_URL"
    echo "deb [trusted=yes] $REPO_URL/ stable main" | sudo tee "$LIST_FILE" > /dev/null
    
    # Update package list
    log "Updating package lists..."
    if sudo apt update; then
        log "Repository added successfully!"
    else
        error "Failed to update package lists. Check your internet connection."
        exit 1
    fi
}

# Show available packages
show_packages() {
    header "Available Packages"
    
    log "Searching for Matrix Creator packages..."
    if apt list 2>/dev/null | grep -q matrixio; then
        apt list matrixio* 2>/dev/null | grep -v "WARNING"
    else
        # Fallback: show what we expect to be available
        log "Matrix Creator kernel modules should be available as:"
        log "  - matrixio-kernel-modules-buster (for Raspbian Buster ARM32)"
        log "  - matrixio-kernel-modules-bookworm (for Raspberry Pi OS Bookworm ARM64)"
    fi
    
    echo
    log "Recommended package for this system: $PACKAGE_NAME"
    log "Platform: $PLATFORM_DESC"
}

# Install packages
install_packages() {
    header "Installing Matrix Creator Kernel Modules"
    
    echo
    log "Recommended package: $PACKAGE_NAME"
    log "Platform: $PLATFORM_DESC"
    echo
    
    read -p "Install the recommended package? (Y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Nn]$ ]]; then
        log "Skipping installation."
        log "You can install manually with: sudo apt install $PACKAGE_NAME"
        return
    fi
    
    log "Installing $PACKAGE_NAME..."
    if sudo apt install -y "$PACKAGE_NAME"; then
        log "Installation completed successfully!"
        
        # Provide post-installation instructions
        echo
        log "Post-installation steps:"
        log "1. Enable the device tree overlay:"
        log "   echo 'dtoverlay=matrixio' | sudo tee -a /boot/config.txt"
        log "2. Reboot to activate the kernel modules"
        echo
        
        # Check if reboot is needed
        if [[ -f /var/run/reboot-required ]]; then
            warn "A reboot is required to complete the installation."
            read -p "Reboot now? (y/N): " -n 1 -r
            echo
            if [[ $REPLY =~ ^[Yy]$ ]]; then
                sudo reboot
            fi
        fi
    else
        error "Installation failed. Check the error messages above."
        log "You can try installing manually with: sudo apt install $PACKAGE_NAME"
        exit 1
    fi
}

# Main function
main() {
    header "Matrix Creator Repository Setup"
    
    check_root
    check_raspberry_pi
    detect_platform
    add_repository
    show_packages
    install_packages
    
    header "Setup Complete"
    log "Matrix Creator repository has been configured."
    log "You can now install updates with: sudo apt update && sudo apt upgrade"
}

# Show usage
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Matrix Creator Debian Repository Setup

OPTIONS:
    --repo-only     Add repository without installing packages
    --help          Show this help message

EXAMPLES:
    $0              # Full setup (add repo + install packages)
    $0 --repo-only  # Add repository only
    
For more information, visit:
$REPO_URL

EOF
}

# Parse command line arguments
case "${1:-}" in
    --help)
        usage
        exit 0
        ;;
    --repo-only)
        check_root
        check_raspberry_pi
        detect_platform
        add_repository
        log "Repository added. Install packages with:"
        log "  sudo apt install matrixio-kernel-modules-buster    (for Raspbian Buster ARM32)"
        log "  sudo apt install matrixio-kernel-modules-bookworm  (for Raspberry Pi OS Bookworm ARM64)"
        ;;
    "")
        main
        ;;
    *)
        error "Unknown option: $1"
        usage
        exit 1
        ;;
esac