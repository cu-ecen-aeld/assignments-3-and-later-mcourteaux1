#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

SCRIPT_WD=$(pwd)

OUTDIR=/home/mike/coursera/aeld/linux/
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    #clean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    #default condig stores to .config
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    #make all
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all 

    #make modules
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

    #make dtbs
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
cp "$OUTDIR/linux-stable/arch/$ARCH/boot/Image" "$OUTDIR/."


echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

mkdir ${OUTDIR}/rootfs

cd  "$OUTDIR/rootfs"
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}

    make distclean
    make defconfig
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
else
    cd busybox
fi

# install busybox
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

#
sudo chmod u+s ${OUTDIR}/rootfs/bin/busybox

echo "Library dependencies"
cd "$OUTDIR/rootfs"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
searchpath=$(${CROSS_COMPILE}gcc --print-sysroot)
find "$searchpath" -name ld-linux-aarch64.so.1 -exec cp {} "$OUTDIR/rootfs/lib/" \;
#copy "Shared library" to rootfs/lib64
find "$searchpath" -name libm.so.6 -exec cp {} "$OUTDIR/rootfs/lib64/" \;
find "$searchpath" -name libresolv.so.2 -exec cp {} "$OUTDIR/rootfs/lib64/" \;
find "$searchpath" -name libc.so.6 -exec cp {} "$OUTDIR/rootfs/lib64/" \;

# TODO: Make device nodes
cd "$OUTDIR/rootfs"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 666 dev/console c 5 1

# TODO: Clean and build the writer utility
cd "$SCRIPT_WD"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp writer "$OUTDIR/rootfs/home"
cp -rH "../conf" "$OUTDIR/rootfs/home"
cp finder-test.sh "$OUTDIR/rootfs/home"
cp finder.sh "$OUTDIR/rootfs/home"
cp autorun-qemu.sh "$OUTDIR/rootfs/home"
sudo chmod +x "$OUTDIR/rootfs/home/finder.sh"

#modify finder-test.sh using conf/ and writer instead writer.sh
sed -i 's|../conf/|conf/|g' ${OUTDIR}/rootfs/home/finder-test.sh
sed -i 's|writer.sh|writer|g' ${OUTDIR}/rootfs/home/finder-test.sh
sed -i 's|./finder.sh|sh finder.sh|g' ${OUTDIR}/rootfs/home/finder-test.sh

# TODO: Chown the root directory
# skipped

# TODO: Create initramfs.cpio.gz
cd "$OUTDIR/rootfs"
find . | cpio -H newc -ov --owner root:root > ../initramfs.cpio
gzip -f ../initramfs.cpio
