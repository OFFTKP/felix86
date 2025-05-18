#!/bin/bash
# Check for curl
if ! command -v curl >/dev/null 2>&1; then
    echo "Error: curl is not installed. Please install it and try again."
    exit 1
fi

# Check for tar
if ! command -v tar >/dev/null 2>&1; then
    echo "Error: tar is not installed. Please install it and try again."
    exit 1
fi

if [ -z "$HOME" ] || [ ! -d "$HOME" ]; then
    echo "Error: \$HOME is not set or not a valid directory."
    exit 1
fi

UBUNTU_2404_ID=1O0nSQAYyQZcAhAOkomSE7Vi7sd211U7a

set -e

echo "Welcome to the felix86 installer"
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
    echo "Downloading Ubuntu 24.04 rootfs..."
    mkdir -p $HOME/felix86_rootfs
    curl -L https://drive.usercontent.google.com/download?id=$UBUNTU_2404_ID&confirm=yep | tar -xz -C $HOME/felix86_rootfs
    felix86 --set-rootfs $HOME/felix86_rootfs
elif [ "$choice" -eq 2 ]; then
    echo "You selected to use your own rootfs."
    echo "Please specify the absolute path to your rootfs"
    read line
    felix86 --set-rootfs line
fi

echo "felix86 installed successfully"