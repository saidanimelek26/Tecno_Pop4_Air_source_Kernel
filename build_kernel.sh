#!/bin/bash
set -e

export ARCH=arm
export SUBARCH=arm

if [ -z "$CROSS_COMPILE" ]; then
    if command -v arm-linux-androideabi-gcc &> /dev/null; then
        export CROSS_COMPILE=arm-linux-androideabi-
    elif command -v arm-eabi-gcc &> /dev/null; then
        export CROSS_COMPILE=arm-eabi-
    elif command -v arm-linux-gnueabi-gcc &> /dev/null; then
        export CROSS_COMPILE=arm-linux-gnueabi-
    elif command -v arm-none-eabi-gcc &> /dev/null; then
        export CROSS_COMPILE=arm-none-eabi-
    fi
fi

DEFCONFIG="tecno_pop4air_lcm_bringup_defconfig"

echo "=========================================================="
echo " Building Minimal LCM Bringup Kernel for Tecno Pop 4 Air  "
echo "=========================================================="
echo "Defconfig:      $DEFCONFIG"
echo "Cross Compile:  $CROSS_COMPILE"
echo "Architecture:   $ARCH"
echo "=========================================================="

make $DEFCONFIG
make -j$(nproc 2>/dev/null || echo 4)

if [ -f arch/arm/boot/zImage-dtb ]; then
    echo "SUCCESS: Output image generated at arch/arm/boot/zImage-dtb"
elif [ -f arch/arm/boot/zImage ]; then
    echo "SUCCESS: Output image generated at arch/arm/boot/zImage"
fi
