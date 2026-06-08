#!/bin/bash
set -e
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
arm-linux-gnueabi-gcc-13 -static -O2 nunchuk_user.c -o nunchuk_user.exe
