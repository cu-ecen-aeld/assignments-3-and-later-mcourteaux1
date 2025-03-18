#!/bin/bash

# Set the CROSS_COMPILE environment variable
export CROSS_COMPILE=arm-linux-gnueabihf-

# Ensure the repository is up-to-date and submodules are initialized
echo "Updating submodules..."
git submodule update --init --recursive

# Check if the directory exists, if not, clone it
if [ ! -d "/tmp/aesd-autograder/linux" ]; then
    echo "Directory /tmp/aesd-autograder/linux does not exist. Cloning the kernel repository..."
    git clone --depth 1 --branch "$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git /tmp/aesd-autograder/linux
else
    echo "Directory /tmp/aesd-autograder/linux exists."
fi

# Check if the directory exists after cloning (if it wasn't there initially)
if [ ! -d "/tmp/aesd-autograder/linux" ]; then
    echo "Directory /tmp/aesd-autograder/linux still does not exist. Exiting..."
    exit 1
fi

# Change to the kernel source directory
cd /tmp/aesd-autograder/linux || { echo "Failed to change directory to /tmp/aesd-autograder/linux"; exit 1; }

# Build the kernel image
echo "Building the kernel image..."
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

