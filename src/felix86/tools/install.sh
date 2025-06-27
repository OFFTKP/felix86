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

if ! command -v sudo >/dev/null 2>&1; then
    echo "Error: sudo is not installed. Please install it and try again."
    exit 1
fi

if [ -z "$HOME" ] || [ ! -d "$HOME" ]; then
    echo "Error: \$HOME is not set or not a valid directory."
    exit 1
fi

if [ -z "$USER" ]; then
    echo "\$USER is not set"
    exit 1
fi

INSTALLATION_DIR="/opt/felix86"
FILE="$INSTALLATION_DIR/felix86"
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
            exit 0
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
sudo mkdir -p $INSTALLATION_DIR

# Save the old installation in /tmp/felix86_artifact/old.<random> in case the user wants it back
TEMP_OLD=$(mktemp -d /tmp/felix86_artifact/old.XXXXXX)
moved_old=0
if [ -f "/usr/bin/felix86" ]; then
    sudo mv /usr/bin/felix86 $TEMP_OLD/felix86.link
    moved_old=1
fi
if [ -f "$FILE" ]; then
    sudo mv $FILE $TEMP_OLD/
    moved_old=1
fi
if [ -f "/usr/bin/felix86-mounter" ]; then
    sudo mv /usr/bin/felix86-mounter $TEMP_OLD/felix86-mounter.link
    moved_old=1
fi
if [ -f "$INSTALLATION_DIR/felix86-mounter" ]; then
    sudo mv $INSTALLATION_DIR/felix86-mounter $TEMP_OLD/
    moved_old=1
fi
if [ -d "$INSTALLATION_DIR/lib" ]; then
    sudo mv $INSTALLATION_DIR/lib $TEMP_OLD/
    moved_old=1
fi

if [[ "$moved_old" == "1" ]]; then
    echo "Moved old felix86 installation to $TEMP_OLD"
fi

sudo mv /tmp/felix86_artifact/felix86 $FILE
sudo mv /tmp/felix86_artifact/felix86-mounter $INSTALLATION_DIR/felix86-mounter
sudo chown root:root $INSTALLATION_DIR/felix86-mounter
sudo chmod u+s $INSTALLATION_DIR/felix86-mounter
sudo mv /tmp/felix86_artifact/lib $INSTALLATION_DIR/
sudo ln -s $FILE /usr/bin/felix86
sudo ln -s $INSTALLATION_DIR/felix86-mounter /usr/bin/felix86-mounter
echo "Successfully installed felix86 at $FILE and libraries at $INSTALLATION_DIR/lib"
felix86 --set-thunks $INSTALLATION_DIR/lib

if [[ "$exit_after_install" == "1" ]]; then
    exit 0
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
    if [ ! -e "$NEW_ROOTFS" ] || [ -d "$NEW_ROOTFS" ] && [ -z "$(ls -A "$NEW_ROOTFS" 2> /dev/null)" ]; then
        echo "Downloading rootfs download link from felix86.com/rootfs/ubuntu.txt..."
        UBUNTU_2404_LINK=$(curl -s https://felix86.com/rootfs/ubuntu.txt)
        echo "Downloading Ubuntu 24.04 rootfs..."
        mkdir -p $NEW_ROOTFS

        # Important we untar with --same-owner so that sudo/mount/fusermount keep their setuid bits
        curl -L $UBUNTU_2404_LINK | sudo tar --same-owner -xz -C $NEW_ROOTFS
        echo "Changing permissions for $NEW_ROOTFS to $USER"

        # Chown the directory so we can add stuff inside, but not recursively as to not ruin setuid stuff
        sudo chown "$USER":"$USER" "$NEW_ROOTFS"
        echo "Rootfs was downloaded and extracted in $NEW_ROOTFS"
        felix86 --set-rootfs $NEW_ROOTFS
    else
        echo "$NEW_ROOTFS already exists and is not empty, I won't unpack the rootfs there"
        exit 1
    fi
elif [ "$choice" -eq 2 ]; then
    echo "You selected to use your own rootfs."
    echo "Please specify the absolute path to your rootfs"
    read line
    felix86 --set-rootfs $line
fi

# Finally register felix86 in binfmt_misc
sudo -E felix86 --binfmt-misc

if ! felix86 -d; then
  echo "Failed binfmt_misc installation check"
fi

echo "felix86 installed successfully"