#!/bin/bash
mkdir -p build
nasm -felf64 -shared ./libwayland-client.asm -o ./build/asm.o
gcc -c -O3 ./libwayland-client.c -o ./build/c.o
gcc -shared -o ./libwayland-client.so ./build/c.o ./build/asm.o