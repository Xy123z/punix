#!/bin/bash

echo "======================================"
echo "Modular Kernel Build Script"
echo "======================================"
echo ""

# Clean
echo "[1/9] Cleaning..."
rm -f *.o *.bin os.bin
echo "    Done!"
echo ""

# Compiler flags
CFLAGS="-m32 -Iinclude -ffreestanding -nostdlib -fno-pie -fno-pic -fno-stack-protector -O2"

# Compile each module
echo "[2/9] Compiling string.c..."
gcc $CFLAGS -c src/string.c -o string.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[3/9] Compiling vga.c..."
gcc $CFLAGS -c src/vga.c -o vga.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[4/9] Compiling memory.c..."
gcc $CFLAGS -c src/memory.c -o memory.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[5/9] Compiling interrupt.c..."
gcc $CFLAGS -c src/interrupt.c -o interrupt.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[6/9] Compiling shell.c..."
gcc $CFLAGS -c src/shell.c -o shell.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[6/9] Compiling text.c..."
gcc $CFLAGS -c src/text.c -o text.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[7/9] Compiling kernel.c..."
gcc $CFLAGS -c kernel.c -o kernel.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[8/9] Linking kernel..."
ld -m elf_i386 -Ttext 0x10000 --oformat binary \
   string.o vga.o memory.o interrupt.o shell.o text.o kernel.o \
   -o kernel.bin -nostdlib -e _start

if [ $? -ne 0 ]; then
    echo "Error: Linking failed!"
    exit 1
fi

echo "[9/9] Assembling bootloader..."
nasm -f bin boot.asm -o boot.bin
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[10/10] Creating OS image..."
cat boot.bin kernel.bin > os.bin

echo ""
echo "======================================"
echo "Build complete!"
echo "Kernel size: $(ls -lh kernel.bin | awk '{print $5}')"
echo "======================================"
echo ""
echo "Running with: qemu-system-i386 -fda os.bin -boot a"
qemu-system-i386 -fda os.bin -boot a
