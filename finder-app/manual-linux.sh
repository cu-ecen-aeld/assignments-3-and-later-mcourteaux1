#!/bin/bash

# Ensure the cross-compiler and necessary dependencies are installed
echo "Installing cross-compiler and dependencies..."
sudo apt-get update
sudo apt-get install -y gcc-arm-linux-gnueabihf qemu qemu-system-arm

# Set the CROSS_COMPILE environment variable
export CROSS_COMPILE=arm-linux-gnueabihf-

# Ensure the repository is up-to-date and submodules are initialized
echo "Updating submodules..."
git submodule update --init --recursive

# Build the kernel image
echo "Building the kernel image..."
cd /tmp/aesd-autograder/linux
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE defconfig
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$(nproc) Image

# Check if Image is built successfully
if [ ! -f /tmp/aesd-autograder/linux/arch/arm/boot/Image ]; then
    echo "Kernel image not found! Exiting..."
    exit 1
fi

# Copy the image to the expected directory
echo "Copying the kernel image..."
sudo cp /tmp/aesd-autograder/linux/arch/arm/boot/Image /tmp/aesd-autograder/

# Build BusyBox
echo "Building BusyBox..."
cd /tmp/aesd-autograder/busybox
make CROSS_COMPILE=arm-linux-gnueabihf- defconfig
make CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)

# Verify if BusyBox is built correctly
if [ ! -f /tmp/aesd-autograder/busybox/busybox ]; then
    echo "BusyBox build failed! Exiting..."
    exit 1
fi

echo "Kernel image and BusyBox successfully built."

# Run the full test
echo "Running full test..."
cd /tmp/aesd-autograder
./full-test.sh

