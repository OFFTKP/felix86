#!/bin/bash
# TODO: fixup output names here
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
gcc -shared -s -o ./libGLX.so.0 ./build/glxc.o ./build/glxasm.o -lX11 # pull in X11 too for XSync and XGetVisualInfo
patchelf --set-soname libGLX.so.0 ./libGLX.so.0

nasm -felf64 -shared ./libGL.asm -o ./build/glasm.o
# Yeah for some reason we need all the libGLX symbols in libGL too
gcc -shared -s -o ./libGL.so.1 ./build/glxc.o ./build/glasm.o -lX11 # pull in X11 too for XSync and XGetVisualInfo
patchelf --set-soname libGL.so.1 ./libGL.so.1

# Build libluajit-5.1 thunk
nasm -felf64 -shared ./libluajit-5.1.asm -o ./build/jasm.o
gcc -c -O3 ./libluajit-5.1.c -o ./build/lua.o
gcc -shared -s -o ./libluajit-5.1.so ./build/lua.o ./build/jasm.o
patchelf --set-soname libluajit-5.1.so ./libluajit-5.1.so

# Build libvdso for x86-64
nasm -felf64 -shared ./x64-linux-vdso.asm -o ./build/vdso64.o
gcc -nostdlib -shared -s -Wl,-soname,linux-vdso.so.1 -Wl,-T,vdso-version.map  -o ./x64-linux-vdso.so.1 ./build/vdso64.o
bin2c -o x64-linux-vdso.h x64-linux-vdso.so.1