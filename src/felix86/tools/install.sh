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

if [ -z "$HOME" ] || [ ! -d "$HOME" ]; then
    echo "Error: \$HOME is not set or not a valid directory."
    exit 1
fi

FILE="/usr/bin/felix86"
FELIX86_LINK="https://nightly.link/OFFTKP/felix86/workflows/unit-tests/master/Linux%20executable.zip"

set -e

echo "Welcome to the felix86 installer"

if [ -f "$FILE" ]; then
    while true; do
        ok=0
        read -p "Another felix86 installation exists. Do you want to reinstall? (yes/no): " answer
        case "$answer" in
            [Yy][Ee][Ss]|[Yy])
                ok=1
                break
                ;;
            [Nn][Oo]|[Nn])
                exit
                ;;
            *)
                echo "Invalid input. Please enter yes or no."
                ;;
        esac

        if [ "$ok" -eq 1 ]; then
            break
        fi
    done
fi

echo "Downloading latest felix86 artifact..."
mkdir -p /tmp/felix86_artifact
curl -sL $FELIX86_LINK -o /tmp/felix86_artifact/archive.zip
unzip -o -d /tmp/felix86_artifact /tmp/felix86_artifact/archive.zip
rm /tmp/felix86_artifact/archive.zip
echo "Downloaded"
echo "Moving felix86 artifact to /usr/bin/, requesting permission..."
sudo mv /tmp/felix86_artifact/felix86 /usr/bin/
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
    UBUNTU_2404_LINK=$(curl -s https://felix86.com/rootfs/ubuntu.txt)
    echo "Downloading Ubuntu 24.04 rootfs..."
    mkdir -p $NEW_ROOTFS
    curl -sL $UBUNTU_2404_LINK | tar -xz -C $NEW_ROOTFS
    echo "Rootfs was downloaded and extracted in $NEW_ROOTFS"
    felix86 --set-rootfs $NEW_ROOTFS
elif [ "$choice" -eq 2 ]; then
    echo "You selected to use your own rootfs."
    echo "Please specify the absolute path to your rootfs"
    read line
    felix86 --set-rootfs $line
fi

echo "felix86 installed successfully"