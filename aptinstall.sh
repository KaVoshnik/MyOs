#!/bin/bash

echo "Installing dependencies for building MyOS..."

sudo apt update -y

# base
sudo apt install -y build-essential git wget curl

# NASM
sudo apt install -y nasm

# QEMU
sudo apt install -y qemu-system-x86

# GRUB
sudo apt install -y grub-pc-bin grub-common

# genisoimage (need for grub-mkrescue)
sudo apt install -y genisoimage

# cross-compiler для x86_64-elf
# If the package is not found, we compile it manually (optional)
if ! command -v x86_64-elf-gcc &> /dev/null; then
    echo "⚠️  x86_64-elf-gcc Not found. Installing from source..."
    mkdir -p ~/tools
    cd ~/tools

    # binutils
    wget https://ftp.gnu.org/gnu/binutils/binutils-2.43.tar.gz
    tar -xzf binutils-2.43.tar.gz
    mkdir -p binutils-build
    cd binutils-build
    ../binutils-2.43/configure --target=x86_64-elf --prefix=/usr/local --disable-nls --disable-werror
    make -j$(nproc)
    sudo make install
    cd ..

    # GCC
    wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
    tar -xzf gcc-13.2.0.tar.gz
    mkdir -p gcc-build
    cd gcc-build
    ../gcc-13.2.0/configure --target=x86_64-elf --prefix=/usr/local --disable-nls --enable-languages=c,c++ --without-headers
    make all-gcc -j$(nproc)
    sudo make install-gcc
    cd ..
fi

echo "Checking installed tools:"
command -v nasm && echo "✅ NASM installed"
command -v qemu-system-x86_64 && echo "✅ QEMU installed"
command -v grub-mkrescue && echo "✅ GRUB installed"
command -v x86_64-elf-gcc && echo "✅ Cross-compiler installed"

echo "All dependencies are installed!"