#!/bin/bash

echo "======================================"
echo "Modular Kernel Build Script"
echo "======================================"
echo ""

# Clean
echo "[1/10] Cleaning..."
rm -f *.o *.bin os.bin
echo "    Done!"
echo ""

# Compiler flags
CFLAGS="-m32 -Iinclude -ffreestanding -nostdlib -fno-pie -fno-pic -fno-stack-protector -O2"

# Compile each module
echo "[2/10] Compiling string.c..."
gcc $CFLAGS -c src/string.c -o string.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[3/10] Compiling vga.c..."
gcc $CFLAGS -c src/vga.c -o vga.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[4/10] Compiling memory.c..."
gcc $CFLAGS -c src/memory.c -o memory.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[5/10] Compiling interrupt.c..."
gcc $CFLAGS -c src/interrupt.c -o interrupt.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[6/10] Compiling shell.c..."
gcc $CFLAGS -c src/shell.c -o shell.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[7/10] Compiling fs.c..."
gcc $CFLAGS -c src/fs.c -o fs.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[8/10] Compiling text.c..."
gcc $CFLAGS -c src/text.c -o text.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[8/10] Compiling console.c..."
gcc $CFLAGS -c src/console.c -o console.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[8/10] Compiling mouse.c..."
gcc $CFLAGS -c src/mouse.c -o mouse.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[9/10] Compiling kernel.c..."
gcc $CFLAGS -c kernel.c -o kernel.o
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[10/10] Linking kernel..."
ld -m elf_i386 -Ttext 0x10000 --oformat binary \
   string.o vga.o memory.o interrupt.o shell.o fs.o text.o console.o mouse.o kernel.o \
   -o kernel.bin -nostdlib -e _start

if [ $? -ne 0 ]; then
    echo "Error: Linking failed!"
    exit 1
fi

echo "[11/10] Assembling bootloader..."
nasm -f bin boot.asm -o boot.bin
if [ $? -ne 0 ]; then echo "Error!"; exit 1; fi

echo "[12/10] Creating OS image..."
cat boot.bin kernel.bin > os.bin

echo ""
echo "======================================"
echo "Build complete!"
echo "Kernel size: $(ls -lh kernel.bin | awk '{print $5}')"
echo "======================================"
echo ""
echo "Running with: qemu-system-i386 -fda os.bin -boot a"
qemu-system-i386 -fda os.bin -boot a
