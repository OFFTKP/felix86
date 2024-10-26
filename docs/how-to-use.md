# How to use

felix86 requires an x86-64 (aka AMD64) "rootfs" which is the filesystem at the root directory on Linux.

The way to get the rootfs varies for each OS, for example for Ubuntu you can use the following link:
- [http://cdimage.ubuntu.com/ubuntu-base/releases/](http://cdimage.ubuntu.com/ubuntu-base/releases/)

After acquiring the rootfs, you need to supply felix86 with the path to the rootfs directory using the `-p` parameter.

After providing the path you can add more optional arguments and finish it with the path to the binary you want to emulate and
any arguments you want to pass.

The binary **must** be inside the rootfs directory, so place it anywhere in there.

Example:
`./felix86 -p /home/myuser/myrootfs /home/myuser/myrootfs/MyApplication arg1 arg2 arg3`

By default, no environment variables are passed to the executable.
You can use `-e` to pass all of your host systems environment variables to the application.

You can use `-t` to strace the emulated application.
