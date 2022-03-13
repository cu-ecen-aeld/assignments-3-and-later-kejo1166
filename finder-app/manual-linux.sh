#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

# KJ:  Modify to complete assignment 3 part 2

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
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

# Check if the dirctory exist; if not, then create directory
if [ ! -d "${OUTDIR}" ]; then
   mkdir -p ${OUTDIR} # Create directory
   
   if [ ! -d "${OUTDIR}" ]; then
      echo "Errot creating directory ${OUTFIR} "
      exit 1
   fi
fi

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

    # TODO: Add your kernel build steps here
    echo "<----- deep clean the kernel build tree - removing the .config file with any existing configurations ----->" 
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper

    echo "<----- Configure for our “virt” arm dev board we will simulate in QEMU ----->"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    echo "<----- Build a kernel image for booting with QEMU ----->"
    make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

    echo "<----- Build any kernel modules ----->"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

    echo "<----- Build the devicetree ----->"
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi 

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
echo "<----- Creating base directories ----->"
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "<----- Busybox: clean and configure ----->" 
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} distclean
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    
else
    cd busybox
fi

# TODO: Make and insatll busybox
echo "<----- Busybox: build and install ----->"
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# Need to change to rootfs before running next commands
cd ${OUTDIR}/rootfs

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
echo "<----- Adding library dependencies ----->"
export SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
cp -L $SYSROOT/lib/ld-linux-aarch64.* lib
cp -L $SYSROOT/lib64/libm.so.* lib64
cp -L $SYSROOT/lib64/libresolv.so.* lib64
cp -L $SYSROOT/lib64/libc.so.* lib64

# TODO: Make device nodes
echo "<----- Creating device nodes ----->"
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1 

# TODO: Clean and build the writer utility
echo "<----- Cleaning and build writer app ----->"
cd $FINDER_APP_DIR
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
echo "<----- Copying finder scripts and executables ----->"
cp -f $FINDER_APP_DIR/autorun-qemu.sh ${OUTDIR}/rootfs/home
cp -f $FINDER_APP_DIR/conf/ -r ${OUTDIR}/rootfs/home
cp -f $FINDER_APP_DIR/finder.sh ${OUTDIR}/rootfs/home
cp -f $FINDER_APP_DIR/finder-test.sh ${OUTDIR}/rootfs/home
cp -f $FINDER_APP_DIR/writer ${OUTDIR}/rootfs/home

# TODO: Chown the root directory
echo "<----- Setting root filesystem owner and group ----->"
cd ${OUTDIR}/rootfs
sudo chown -R root:root *

# TODO: Create initramfs.cpio.gz
echo "<----- Creating initramfs ----->"
cd ${OUTDIR}/rootfs
if [ -e "${OUTDIR}/initramfs.cpio" ];then
    rm ${OUTDIR}/initramfs.cpio
fi
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio

echo "<----- Compressing  ${OUTDIR}/initramfs.cpio ----->"
cd ${OUTDIR}
if [ -e "initramfs.cpio.gz" ];then
    rm initramfs.cpio.gz
fi
gzip -f initramfs.cpio # compress