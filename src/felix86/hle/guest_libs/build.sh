#!/bin/bash

# Build libwayland-client thunk
mkdir -p build
nasm -felf64 -shared ./libwayland-client.asm -o ./build/asm.o
gcc -c -O3 ./libwayland-client.c -o ./build/c.o
gcc -shared -s -o ./libwayland-client.so ./build/c.o ./build/asm.o
patchelf --set-soname libwayland-client.so.0 ./libwayland-client.so

# Build libvulkan thunk
nasm -felf64 -shared ./libvulkan.asm -o ./build/vasm.o
gcc -shared -s -o ./libvulkan.so.1 ./build/vasm.o
patchelf --set-soname libvulkan.so.1 ./libvulkan.so.1
