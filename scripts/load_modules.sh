#!/bin/bash

# Matrix Creator Module Loading Script for Raspberry Pi 5
# Loads modules in correct dependency order

echo "Loading Matrix Creator kernel modules..."

# Load IIO subsystem first
sudo modprobe industrialio
echo "✓ IIO subsystem loaded"

# Load core modules
cd ~/matrixio-kernel-modules/src
sudo insmod matrixio-core.ko
sudo insmod matrixio-regmap.ko
echo "✓ Core modules loaded"

# Load sensor modules  
sudo insmod matrixio-env.ko
sudo insmod matrixio-imu.ko
echo "✓ Sensor modules loaded"

# Load interface modules
sudo insmod matrixio-everloop.ko
echo "✓ LED module loaded"

# Load audio modules
sudo insmod matrixio-codec.ko
sudo insmod matrixio-mic.ko  
sudo insmod matrixio-playback.ko
echo "✓ Audio modules loaded"

echo "Matrix Creator modules loaded successfully!"
echo "Check with: lsmod | grep matrixio"
