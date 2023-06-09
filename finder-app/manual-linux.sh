#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u


#echo "Enter the diroctory path: "
#read OUTDIR
#echo "This is a file" > "${OUTDIR}"
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

    # TODO: Add your kernel build steps here
    make ARCH=arm64\
     CROSS_COMPILE=${CROSS_COMPILE} mrproper #deep clean
    make ARCH=arm64\
     CROSS_COMPILE=${CROSS_COMPILE} defconfig #create virt qemu
    make -j4 ARCH=arm64\
     CROSS_COMPILE=${CROSS_COMPILE} all #build all
    make ARCH=arm64\
     CROSS_COMPILE=${CROSS_COMPILE} modules #create modules
    make ARCH=arm64\
     CROSS_COMPILE=${CROSS_COMPILE} dtbs #

    echo KERNEL BUILD DONE
fi

echo "Adding the Image in outdir"
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "${OUTDIR}"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf "${OUTDIR}/rootfs"
fi

# TODO: Create necessary base directories

mkdir -p "${OUTDIR}/rootfs"
cd "${OUTDIR}/rootfs"
mkdir -p etc lib lib64 bin sbin dev home sys proc tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]
then
git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Start Cleaning the Made Settings"
    make distclean
     echo "Start Making Settings"   
    make defconfig
    echo "Finished Busybox Configuration"
else
    cd "${OUTDIR}/busybox"
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    echo "Start Cleaning the Made Settings"
    make distclean
     echo "Start Making Settings"   
    make defconfig
    echo "Finished Busybox Configuration"
fi

# TODO: Make and install busybox
echo "Start Make busybox"
make ARCH=${ARCH}\
    CROSS_COMPILE=${CROSS_COMPILE}
echo "FIRST TASK DONE"
make CONFIG_PREFIX=${OUTDIR}/rootfs\
    ARCH=${ARCH}\
    CROSS_COMPILE=${CROSS_COMPILE} install
echo "Finished Busybox Make & Install"

cd "${OUTDIR}/rootfs"

echo "Library Dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
echo $SYSROOT
cp ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
cp ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
cp ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64
echo "Adding libraries DONE"
# TODO: Make device nodes
echo "MAKE node Start"
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1
echo "MAKE node DONE"
# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make all CROSS_COMPILE=${CROSS_COMPILE}
echo "CLEAN AND BUILD WRITER DONE"
# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
cp ${FINDER_APP_DIR}/finder.sh ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/../conf/username.txt ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/finder-test.sh ${OUTDIR}/rootfs/home
cp ${FINDER_APP_DIR}/autorun-qemu.sh ${OUTDIR}/rootfs/home
echo "DONE COPY FILES /HOME"
# TODO: Chown the root directory
cd ${OUTDIR}/rootfs
sudo chown -R root:root *
echo "chown DONE"
# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
if [ ! -d "/tmp/aeld"]
then
    mkdir "/tmp/aeld"
    cp -r /tmp/aesd-autograder/* /tmp/aeld
fi