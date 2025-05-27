#!/bin/bash

set -e

mkdir -p build

# Build libwayland-client thunk
nasm -felf64 -shared ./libwayland-client.asm -o ./build/asm.o
gcc -c -O3 ./libwayland-client.c -o ./build/c.o
gcc -shared -s -o ./libwayland-client.so ./build/c.o ./build/asm.o
patchelf --set-soname libwayland-client.so.0 ./libwayland-client.so

# Build libvulkan thunk
nasm -felf64 -shared ./libvulkan.asm -o ./build/vasm.o
gcc -shared -s -o ./libvulkan.so.1 ./build/vasm.o
patchelf --set-soname libvulkan.so.1 ./libvulkan.so.1

nasm -felf64 -shared ./libGLX.asm -o ./build/glxasm.o
gcc -fPIC -c -O3 ./libGLX.c -o ./build/glxc.o
gcc -shared -s -o ./libGLX.so.0 ./build/glxc.o ./build/glxasm.o
patchelf --set-soname libGLX.so.0 ./libGLX.so.0
