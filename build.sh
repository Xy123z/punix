#!/bin/bash
echo "======================================"
echo "Modular Kernel Build Script"
echo "======================================"
echo ""

# Clean
echo "[1/12] Cleaning..."
rm -f *.o *.bin os.bin disk.img
echo "    Done!"
echo ""

# Compiler flags
CFLAGS="-m32 -Iinclude -ffreestanding -nostdlib -fno-pie -fno-pic -fno-stack-protector -O2"

# Compile each module

echo "[6/12] Compiling shell.c..."
gcc $CFLAGS -c src/shell.c -o shell.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[2/12] Compiling syscall.c..."
gcc $CFLAGS -c src/syscall.c -o syscall.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[2/12] Compiling string.c..."
gcc $CFLAGS -c src/string.c -o string.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[3/12] Compiling vga.c..."
gcc $CFLAGS -c src/vga.c -o vga.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[4/12] Compiling memory.c..."
gcc $CFLAGS -c src/memory.c -o memory.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[5/12] Compiling interrupt.c..."
gcc $CFLAGS -c src/interrupt.c -o interrupt.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[6/12] Compiling shell.c..."
gcc $CFLAGS -c src/shell.c -o shell.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[7/12] Compiling fs.c..."
gcc $CFLAGS -c src/fs.c -o fs.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[8/12] Compiling text.c..."
gcc $CFLAGS -c src/text.c -o text.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[9/12] Compiling console.c..."
gcc $CFLAGS -c src/console.c -o console.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[10/12] Compiling mouse.c..."
gcc $CFLAGS -c src/mouse.c -o mouse.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[11/12] Compiling ata.c..."
gcc $CFLAGS -c src/ata.c -o ata.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[12/12] Compiling math.c..."
gcc $CFLAGS -c src/math.c -o math.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi


echo "[12/12] Compiling auth.c..."
gcc $CFLAGS -c src/auth.c -o auth.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[13/12] Compiling kernel.c..."
gcc $CFLAGS -c kernel.c -o kernel.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[14/12] Linking kernel..."
ld -m elf_i386 -Ttext 0x10000 --oformat binary \
   kernel.o string.o vga.o memory.o interrupt.o shell.o fs.o text.o console.o mouse.o ata.o math.o auth.o syscall.o\
   -o kernel.bin -nostdlib -e _start
if [ $? -ne 0 ]; then
    echo "Error: Linking failed!"
    exit 1
fi

# Calculate actual kernel size in sectors (round up)
KERNEL_SIZE=$(stat -f%z kernel.bin 2>/dev/null || stat -c%s kernel.bin)
KERNEL_SECTORS=$(( ($KERNEL_SIZE + 511) / 512 ))

echo ""
echo "Kernel size: $KERNEL_SIZE bytes ($KERNEL_SECTORS sectors)"
echo ""

# Check if kernel is too large
if [ $KERNEL_SECTORS -gt 200 ]; then
    echo "WARNING: Kernel is very large ($KERNEL_SECTORS sectors)"
    echo "Consider increasing the filesystem start sector in fs.h"
fi

echo "[15/12] Assembling bootloader..."
nasm -f bin boot.asm -o boot.bin
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[16/12] Creating OS image..."

# Create a 50MB disk image
dd if=/dev/zero of=disk.img bs=1M count=50 status=none
if [ $? -ne 0 ]; then echo "Error creating disk image!"; exit 1; fi

# Write the bootloader to LBA Sector 0 (CHS Sector 1)
dd if=boot.bin of=disk.img seek=0 count=1 bs=512 conv=notrunc status=none
if [ $? -ne 0 ]; then echo "Error writing bootloader!"; exit 1; fi

# Write the kernel starting at LBA Sector 1 (CHS Sector 2)
# This corresponds to CHS: Cylinder 0, Head 0, Sector 2
dd if=kernel.bin of=disk.img seek=1 bs=512 conv=notrunc status=none
if [ $? -ne 0 ]; then echo "Error writing kernel!"; exit 1; fi

echo ""
echo "======================================"
echo "Build complete!"
echo "Kernel: $KERNEL_SIZE bytes ($KERNEL_SECTORS sectors)"
echo "Bootloader: 512 bytes (1 sector)"
echo "Filesystem starts at sector: $((KERNEL_SECTORS + 1))"
echo "======================================"
echo ""

# Launch QEMU
echo "Launching QEMU..."
qemu-system-i386 -drive file=disk.img,format=raw,index=0,media=disk -boot c
