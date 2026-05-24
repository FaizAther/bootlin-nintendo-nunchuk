#!/bin/bash
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)


arm-linux-gnueabi-gcc-13 -static serial_user.c -o serial_user.exe