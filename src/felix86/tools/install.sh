#!/bin/bash
arch=$(uname -m)

if [ "$arch" != "riscv64" ]; then
    echo "You are not on 64-bit RISC-V. felix86 only works on 64-bit RISC-V."
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "Error: curl is not installed. Please install it and try again."
    exit 1
fi

if ! command -v tar >/dev/null 2>&1; then
    echo "Error: tar is not installed. Please install it and try again."
    exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
    echo "Error: unzip is not installed. Please install it and try again."
    exit 1
fi

if [ -z "$HOME" ] || [ ! -d "$HOME" ]; then
    echo "Error: \$HOME is not set or not a valid directory."
    exit 1
fi

FILE="/usr/bin/felix86"
FELIX86_LINK="https://nightly.link/OFFTKP/felix86/workflows/unit-tests/master/Linux%20executable.zip"

set -e

echo "Welcome to the felix86 installer"

exit_after_install=0

if [ -f "$FILE" ]; then
    echo "There's already an installation at $FILE. What would you like to do?"
    echo "(1) Update with latest artifact"
    echo "(2) Full reinstall"
    echo "(3) Exit"

    while true; do
        read -p "Your choice: " choice
        if [[ "$choice" == "1" ]]; then
            exit_after_install=1
            break
        elif [[ "$choice" == "2" ]]; then
            break
        elif [[ "$choice" == "3" ]]; then
            exit
        else
            echo "Invalid input. Please enter 1, 2 or 3"
        fi
    done
fi

echo "Downloading latest felix86 artifact..."
mkdir -p /tmp/felix86_artifact
curl -L $FELIX86_LINK -o /tmp/felix86_artifact/archive.zip
unzip -o -d /tmp/felix86_artifact /tmp/felix86_artifact/archive.zip
rm /tmp/felix86_artifact/archive.zip
echo "Downloaded"
echo "Moving felix86 artifact to $FILE, requesting permission..."
sudo mv /tmp/felix86_artifact/felix86 $FILE
echo "Successfully installed felix86 at $FILE"

if [[ "$exit_after_install" == "1" ]]; then
    exit
fi

echo ""
echo "Which rootfs would you like to use?"
echo "(1) Ubuntu 24.04"
echo "(2) I have my own rootfs"

while true; do
    read -p "Your choice: " choice
    if [[ "$choice" == "1" || "$choice" == "2" ]]; then
        break
    else
        echo "Invalid input. Please enter 1 or 2."
    fi
done


if [ "$choice" -eq 1 ]; then
    echo "Where do you want to extract the downloaded rootfs?"
    read NEW_ROOTFS
    if [ -e "$NEW_ROOTFS" ]; then
        echo "$NEW_ROOTFS already exists, I couldn't unpack the rootfs there"
        exit
    fi
    echo "Downloading rootfs download link from felix86.com/rootfs/ubuntu.txt..."
    UBUNTU_2404_LINK=$(curl -s https://felix86.com/rootfs/ubuntu.txt)
    echo "Downloading Ubuntu 24.04 rootfs..."
    mkdir -p $NEW_ROOTFS
    curl -L $UBUNTU_2404_LINK | tar -xmz -C $NEW_ROOTFS
    echo "Rootfs was downloaded and extracted in $NEW_ROOTFS"
    felix86 --set-rootfs $NEW_ROOTFS
elif [ "$choice" -eq 2 ]; then
    echo "You selected to use your own rootfs."
    echo "Please specify the absolute path to your rootfs"
    read line
    felix86 --set-rootfs $line
fi

echo "felix86 installed successfully"