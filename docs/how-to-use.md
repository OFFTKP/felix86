# How to use

> [!IMPORTANT]
> felix86 is early in development. It can run some games, see https://felix86.com/compat/.
>
> Currently the emulator is only tested on boards with **VLEN=256**

## Required architecture

You need a RISC-V board with `rv64gv` extensions.

Furthermore, **you need a recent version of Linux like `6.6`**, so that there is vector extension support in signal handlers.
If you don't have a recent version of Linux, things may go wrong.

Any extra extensions might be utilized, but `G` and `V` are mandatory.

felix86 is going to tell you which extensions it detects on your system.
If you have an extension but it's unable to detect it, you can use the environment variable:
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

You can also cross-compile, since compiling on RISC-V might be slow:

```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=riscv.cmake
cmake --build build -j$(nproc)
```

Make sure to [grab a RootFS](#rootfs), set the `FELIX86_ROOTFS` environment variable, and then felix86 is ready to run!

## QEMU

This works fine for me: (change the cores/RAM to your liking)

Make sure the disk image has enough space to compile.
```bash
qemu-system-riscv64 \
-machine virt -m 8192 -smp 10 \
-cpu rv64,v=true,vlen=128,vext_spec=v1.0,zacas=true,zabha=true,zba=true,zbb=true,zbc=true,zbs=true \
-bios /usr/share/qemu/opensbi-riscv64-generic-fw_dynamic.bin \
-kernel /usr/share/u-boot-qemu-bin/qemu-riscv64_smode/uboot.elf \
-device virtio-net-device,netdev=eth0 -netdev user,id=eth0 \
-device virtio-rng-pci \
-drive file=ubuntu-24.04.1-preinstalled-server-riscv64.img,format=raw,if=virtio
```

## RootFS

felix86 requires an x86-64 "rootfs" which is the filesystem at the root directory on Linux, this is needed for x86-64 libraries and tools used by the games you are going to run

> [!TIP]
> You can use `felix86 --set-rootfs /path/to/rootfs` to easily set the rootfs directory in the config.toml

### Downloading a rootfs

Ready-made rootfs images are uploaded to Google Drive. Currently there's only one image, you can obtain the link from [https://felix86.com/rootfs/ubuntu.txt]. Download the image and decompress it to a folder. That folder is now your rootfs.

The images are built using the scripts in [https://github.com/felix86-emu/rootfs] and manually uploaded to Google Drive for distribution.

### Building your own rootfs

Clone [https://github.com/felix86-emu/rootfs] and run the `BuildAll.sh` script.

After acquiring the rootfs, you need to supply felix86 with the path to the rootfs directory using the `FELIX86_ROOTFS` environment variable.


## Configuration

View `$HOME/.config/felix86/config.toml` for configurable options and their descriptions.
felix86 default configurations are relatively conservative, but some adjustments may be needed for certain games.

> [!TIP]
> View [https://github.com/felix86-emu/compatibility-list/issues/] to see if the game you want to run is supported
> and if there's an special configuration necessary.

### Thunking

> [!WARNING]
> Thunking support is not great yet. Some games may not work with thunking enabled.

On systems with a GPU that has no x86-64 drivers (for example any board with a PowerVR iGPU) you may be unable to use your GPU without thunking. Thunking enables using some RISC-V libraries instead of x86-64 libraries.

To enable thunking, set the environment variable `FELIX86_THUNKS=/path/to/felix86/src/felix86/hle/guest_libs`

Or the respective variable in `$HOME/.config/felix86/config.toml`

Want to disable thunking? `export FELIX86_ENABLED_THUNKS=` will do the trick -- or you can unset the `FELIX86_THUNKS` path.

Want to thunk Vulkan but not EGL? You can do so with `FELIX86_ENABLED_THUNKS=vulkan,wayland`


## Running a game

The game you want to run **must** be inside the rootfs directory, so place it anywhere in there.

Example:
`./felix86 /home/myuser/myrootfs/MyDir/MyApplication arg1 arg2 arg3`

Or, don't prepend the executable path with the rootfs path:
`./felix86 /MyDir/MyApplication arg1 arg2 arg3`

By default, the host environment variables are passed to the executable.

You can find log files from runs of the emulator in `/tmp/felix86-XXXXXX.log`

Use `--help` to view all the options.

## Compiling tests

Set `BUILD_TESTS` to 1/ON/whatever CMake recognizes as truthy.

Run `felix86_test` to run every test, or `felix86_test "the test name"` to run a specific test.

Also try `felix86_test --help`, it uses Catch2.

## Specifying available extensions
felix86 will use Linux's riscv_hwprobe syscall to try to find the available RISC-V extensions.

There are cases where you might want to override which extensions are available entirely. For example, if your board has `g,c,v,zicond` you might want felix86 to only use the `g,c,v` extensions. Or you might want to specify that you have `XTheadCondMov`, which is undetectable via that syscall. This can be achieved with either the command line option `--all-extensions` or the environment variable `FELIX86_ALL_EXTENSIONS`.

If you want to only add new extensions instead of overriding them, use the environment variable `FELIX86_EXTENSIONS`

felix86 requires `g` and `v` with vlen of at least 128.
