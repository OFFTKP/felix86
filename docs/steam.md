# Running Steam on felix86

> [!IMPORTANT]
> Make sure to get the felix86 nightly version for now, using the installation script
>
> Older versions **won't** be able to run Steam

> [!WARNING]
> **Make sure you have a swap file!**
>
> You may experience freezes if you run out of memory

## Step 1 - Download
- Download the `steam_latest.deb` package from Steam's website

## Step 2 - Installation
- Move the package you just downloaded inside your rootfs
- Enter felix86 bash (`felix86 /bin/bash`) and install the package using `sudo dpkg -i ./steam_latest.deb`

## Step 3 - Running
- If you're on wayland, run `export SDL_VIDEODRIVER=x11` as it may not work on Wayland currently
- Run `steam -no-cef-sandbox`
  - If you run into problems, you can also try the flag `-cef-disable-gpu`
- The download and installation will take about 15 minutes

# Running Wine on felix86

> [!WARNING]
> 32-bit wine may run into problems
>
> Check the compatibility list

The rootfs downloaded from the installation script has both 32-bit and 64-bit wine.

Enter the bash environment and run `wine` with the `.exe` of your choice.