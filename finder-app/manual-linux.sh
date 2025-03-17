#!/bin/bash
set -euo pipefail

# Set output directory (default: /tmp/aeld)
if [ "$#" -gt 0 ]; then
    OUTDIR=$(realpath "$1")  # Convert to absolute path
else
    OUTDIR="/tmp/aeld"
fi

echo "Output directory: $OUTDIR"

# Define versions (use underscores for BusyBox)
BUSYBOX_VERSION="1_35_0"
KERNEL_VERSION="v5.15.163"

# Define cross-compile toolchain
CROSS_COMPILE=${CROSS_COMPILE:-arm-linux-gnueabihf-}

# Ensure output directory exists
mkdir -p "$OUTDIR"

### STEP 1: Setting Up BusyBox ###
echo "Setting up BusyBox..."

# Ensure a clean root filesystem
echo "Deleting rootfs directory at $OUTDIR/rootfs and starting over"
sudo rm -rf "$OUTDIR/rootfs"

echo "Creating rootfs directory at $OUTDIR/rootfs"
mkdir -p "$OUTDIR/rootfs/bin"

# Install BusyBox if it is not already installed in the rootfs
if [ ! -f "$OUTDIR/rootfs/bin/busybox" ]; then
    echo "Installing BusyBox..."
    
    if [ ! -d "$OUTDIR/busybox" ]; then
        echo "Cloning BusyBox repository..."
        git clone git://busybox.net/busybox.git "$OUTDIR/busybox"
    fi

    pushd "$OUTDIR/busybox" > /dev/null

    git fetch --tags
    git checkout "$BUSYBOX_VERSION"

    make distclean
    make defconfig

    echo "Building BusyBox with CROSS_COMPILE=$CROSS_COMPILE..."
    make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$(nproc)
    
    echo "Installing BusyBox into rootfs..."
    make CONFIG_PREFIX="$OUTDIR/rootfs" install

    popd > /dev/null
fi

# Verify BusyBox installation
if [ ! -f "$OUTDIR/rootfs/bin/busybox" ]; then
    echo "Error: BusyBox installation failed!"
    exit 1
fi

echo "BusyBox installation complete."
ls -l "$OUTDIR/rootfs/bin/busybox"

### STEP 2: Building the Kernel ###
echo "Starting Linux Kernel build..."

if [ ! -d "$OUTDIR/linux" ]; then
    echo "Cloning Linux kernel source..."
    git clone --depth 1 --branch "$KERNEL_VERSION" https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git "$OUTDIR/linux"
fi

pushd "$OUTDIR/linux" > /dev/null

echo "Checking out kernel version $KERNEL_VERSION..."
git checkout "$KERNEL_VERSION"

make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE mrproper
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE defconfig

echo "Building the kernel..."
make ARCH=arm CROSS_COMPILE=$CROSS_COMPILE -j$(nproc) Image

echo "Copying kernel image to $OUTDIR..."
cp arch/arm/boot/Image "$OUTDIR/Image"

popd > /dev/null

echo "Kernel build completed successfully!"

### STEP 3: Creating Root Filesystem (`rootfs`) ###
echo "Setting up root filesystem in $OUTDIR/rootfs"

# Create necessary directories
mkdir -p "$OUTDIR/rootfs" \
         "$OUTDIR/rootfs/bin" \
         "$OUTDIR/rootfs/sbin" \
         "$OUTDIR/rootfs/lib" \
         "$OUTDIR/rootfs/lib64" \
         "$OUTDIR/rootfs/dev" \
         "$OUTDIR/rootfs/proc" \
         "$OUTDIR/rootfs/sys" \
         "$OUTDIR/rootfs/tmp" \
         "$OUTDIR/rootfs/home"

# Create device nodes
echo "Creating device nodes..."
sudo mknod -m 666 "$OUTDIR/rootfs/dev/null" c 1 3
sudo mknod -m 600 "$OUTDIR/rootfs/dev/console" c 5 1

# Copy required libraries for BusyBox
echo "Copying shared libraries..."
# Correct library path
SYSROOT="/usr/arm-linux-gnueabihf/lib"

echo "Using fixed sysroot path: $SYSROOT"

# Copy required libraries
echo "Copying shared libraries..."
cp -v "$SYSROOT/ld-linux-armhf.so.3" "$OUTDIR/rootfs/lib/"
cp -v "$SYSROOT/libm.so.6" "$OUTDIR/rootfs/lib/"
cp -v "$SYSROOT/libresolv.so.2" "$OUTDIR/rootfs/lib/"
cp -v "$SYSROOT/libc.so.6" "$OUTDIR/rootfs/lib/"


# Create init script (init will be executed by kernel on boot)
echo "Creating init script..."
cat << EOF | sudo tee "$OUTDIR/rootfs/init"
#!/bin/sh
mount -t proc none /proc
mount -t sysfs none /sys
exec /bin/sh
EOF
sudo chmod +x "$OUTDIR/rootfs/init"

# Create standalone initramfs
echo "Creating initramfs..."
pushd "$OUTDIR/rootfs" > /dev/null
find . | cpio -o --format=newc | gzip > "$OUTDIR/initramfs.cpio.gz"
popd > /dev/null

echo "Root filesystem setup complete!"

# Get the absolute path of the script directory
SCRIPT_DIR=$(dirname "$(realpath "$0")")

echo "Copying required scripts to rootfs/home/..."

mkdir -p "$OUTDIR/rootfs/home/"

# List of files to copy
FILES=("finder.sh" "finder-test.sh" "conf/username.txt" "conf/assignment.txt" "autorun-qemu.sh")

for file in finder.sh finder-test.sh conf/username.txt conf/assignment.txt autorun-qemu.sh; do
    SRC_PATH="$SCRIPT_DIR/$file"
    DEST_PATH="$OUTDIR/rootfs/home/$(basename "$file")"  # Remove conf/ from destination

    if [ -f "$SRC_PATH" ]; then
        cp -v "$SRC_PATH" "$DEST_PATH"
        chmod +x "$DEST_PATH"  # Ensure chmod uses the correct path
    else
        echo "Warning: $file not found in $SCRIPT_DIR! Skipping copy..."
    fi
done


# Copy finder scripts
cp -v "finder.sh" "$OUTDIR/rootfs/home/"
cp -v "conf/username.txt" "$OUTDIR/rootfs/home/"
cp -v "conf/assignment.txt" "$OUTDIR/rootfs/home/"
cp -v "finder-test.sh" "$OUTDIR/rootfs/home/"
cp -v "autorun-qemu.sh" "$OUTDIR/rootfs/home/"

# Ensure correct permissions
chmod +x "$OUTDIR/rootfs/home/finder.sh"
chmod +x "$OUTDIR/rootfs/home/finder-test.sh"
chmod +x "$OUTDIR/rootfs/home/autorun-qemu.sh"

echo "Recreating initramfs with updated files..."
pushd "$OUTDIR/rootfs" > /dev/null
find . | cpio -o --format=newc | gzip > "$OUTDIR/initramfs.cpio.gz"
popd > /dev/null

echo "Copying QEMU startup script..."
cp -v "$SCRIPT_DIR/start-qemu-terminal.sh" "$OUTDIR/"
chmod +x "$OUTDIR/start-qemu-terminal.sh"

