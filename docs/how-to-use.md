# How to use

## Required architecture
You need either an emulator like QEmu or a board with `rv64gv`.
Any extra extensions might be utilized, but `G` and `V` are mandatory.

felix86 is going to tell you which extensions it detects on your system.
If you have an extension but it's unable to detect it, you can use the environment variable:
```
FELIX86_EXTENSIONS=v,b
```
to specify any extra extensions on top of the ones it detects, or:
```
FELIX86_ALL_EXTENSIONS=g,c,v,b
```
to specify all available extensions.

## Using on RISC-V hardware

Simply compile felix86 with CMake

Run these from the base directory of felix86:
```bash
cmake -B build
cmake --build build -j$(nproc)
```

Make sure to [grab a RootFS](#rootfs) and then felix86 is ready to run!

## Using on other architectures

Since RISC-V hardware is not up to par when it comes to both speed and extensions, it's desirable sometimes to use your more powerful hardware
to run felix86 for debugging or testing purposes.

[Spike](https://github.com/riscv-software-src/riscv-isa-sim) seemed like a good candidate to emulate felix86, however the proxy kernel it uses seems quite incomplete right now.

Currently to test felix86 I use qemu-riscv and run an Ubuntu image.

This works fine for me: (change the cores/RAM to your liking)
Make sure the disk image has enough space to compile.
```bash
qemu-system-riscv64 \
-machine virt -m 8192 -smp 10 \
-cpu rv64,v=true,vlen=128,vext_spec=v1.0,zacas=true,zabha=true,zba=true,zbb=true,zbc=true,zbs=true,rvv_ta_all_1s=true,rvv_ma_all_1s=true \
-bios /usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin \
-kernel /usr/share/u-boot-qemu-bin/qemu-riscv64_smode/uboot.elf \
-device virtio-net-device,netdev=eth0 -netdev user,id=eth0 \
-device virtio-rng-pci \
-drive file=ubuntu-24.04.1-preinstalled-server-riscv64.img,format=raw,if=virtio
```

## RootFS

felix86 requires an x86-64 "rootfs" which is the filesystem at the root directory on Linux.

The way to get the rootfs varies for each distro, for Ubuntu you can use the following link:
- [http://cdimage.ubuntu.com/ubuntu-base/releases/](http://cdimage.ubuntu.com/ubuntu-base/releases/)

After acquiring the rootfs, you need to supply felix86 with the path to the rootfs directory using the `-p` parameter.

After providing the path you can add more optional arguments and finish it with the path to the binary you want to emulate and
any arguments you want to pass.

The binary **must** be inside the rootfs directory, so place it anywhere in there.

Example:
`./felix86 -p /home/myuser/myrootfs /home/myuser/myrootfs/MyApplication arg1 arg2 arg3`

By default, no environment variables are passed to the executable.

Use `--help` to view all the options.

## Compiling tests

Set `BUILD_TESTS` to 1/ON/whatever CMake recognizes as truthy.

Run `felix86_test` to run every test, or `felix86_test "the test name"` to run a specific test.

Also try `felix86_test --help`, it uses Catch2.

## Specifying available extensions
felix86 will use linux's riscv_hwprobe syscall to try to find the available RISC-V extensions.

However, that syscall doesn't allow for probing all extensions one might want to use.
Notably, it doesn't check for `Zabha`, `Zam` and `XTheadCondMov`, all of which felix86 can use if available.

You can tell felix86 that you have these extensions using either the command line option `--extensions` or the environment variable `FELIX86_EXTENSIONS`.

These two take a comma separated list of extensions like this:
`zicond,xtheadcondmov,zabha`

There are cases where you might want to override which extensions are available entirely. For example, if your board has `g,c,v,zicond` you might want felix86 to only use
the `g,c,v` extensions. This can be achieved with either the command line option `--all-extensions` or the environment variable `FELIX86_ALL_EXTENSIONS`.

felix86 requires `g` and `v` with vlen of at least 128.