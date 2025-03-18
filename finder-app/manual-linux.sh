#!/bin/bash

# Ensure that the CROSS_COMPILE environment variable is set correctly
export CROSS_COMPILE=arm-linux-gnueabihf-

# Ensure the kernel source exists at /tmp/aesd-autograder/linux
KERNEL_DIR="/tmp/aesd-autograder/linux"
if [ ! -d "$KERNEL_DIR" ]; then
    echo "Kernel source not found at $KERNEL_DIR, cloning..."
    git clone --depth 1 --branch "$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$KERNEL_DIR"
    
    if [ $? -ne 0 ]; then
        echo "Error: Failed to clone the kernel source. Exiting..."
        exit 1
    fi
else
    echo "Kernel source found at $KERNEL_DIR"
fi

# Ensure the permissions are set correctly to allow access to the directory
sudo chmod -R u+w "$KERNEL_DIR"

# Navigate to the kernel source directory
cd "$KERNEL_DIR" || { echo "Error: Failed to change to the kernel directory."; exit 1; }

# Set up the kernel configuration and build the kernel image
echo "Building the kernel image..."
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE defconfig
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$(nproc) Image

# Check if the image is built successfully
if [ ! -f "$KERNEL_DIR/arch/arm/boot/Image" ]; then
    echo "Error: Kernel image not found! Exiting..."
    exit 1
fi

# Copy the built image to the expected location
echo "Copying the kernel image..."
sudo cp "$KERNEL_DIR/arch/arm/boot/Image" /tmp/aesd-autograder/

# Verify if the kernel image exists in the target location
if [ ! -f "/tmp/aesd-autograder/Image" ]; then
    echo "Error: Kernel image copy failed! Exiting..."
    exit 1
fi

# Ensure BusyBox source exists at /tmp/aesd-autograder/busybox
BUSYBOX_DIR="/tmp/aesd-autograder/busybox"
if [ ! -d "$BUSYBOX_DIR" ]; then
    echo "BusyBox source not found at $BUSYBOX_DIR, cloning..."
    sudo git clone https://git.busybox.net/busybox.git "$BUSYBOX_DIR"
    
    if [ $? -ne 0 ]; then
        echo "Error: Failed to clone the BusyBox source. Exiting..."
        exit 1
    fi
else
    echo "BusyBox source found at $BUSYBOX_DIR"
fi


# Navigate to the BusyBox directory and start building
cd "$BUSYBOX_DIR" || { echo "Error: Failed to change to the BusyBox directory."; exit 1; }
make defconfig
make -j$(nproc)

# Verify if BusyBox is built
if [ ! -f "$BUSYBOX_DIR/busybox" ]; then
    echo "Error: BusyBox build failed! Exiting..."
    exit 1
fi

echo "Kernel image and BusyBox successfully built."

# Run the full test (if applicable)
echo "Running full test..."
cd /tmp/aesd-autograder
/home/mike/Desktop/Assignment3/full-test.sh

