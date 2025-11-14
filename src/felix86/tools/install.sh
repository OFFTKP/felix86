#!/bin/bash

### Important: If you update this script, make sure to also update https://felix86.com/install.sh

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

check_url() {
  local url="$1"

  if ! curl -k --output /dev/null --silent --head --fail "$url"; then
    echo "URL is invalid or unreachable: $url"
    exit 1
  else
    return 0
  fi
}

select_release_url() {
  local url_list
  echo "Checking if https://felix86.com/releases.txt is live..."
  check_url "https://felix86.com/releases.txt"
  echo "Downloading release list..."
  url_list=$(curl -k -s https://felix86.com/releases.txt)
  echo "Downloaded"

  if [[ -z "$url_list" ]]; then
    echo "Failed to fetch the releases list or it's empty."
    return 1
  fi

  local total_lines
  total_lines=$(echo "$url_list" | wc -l)
  if (( total_lines > 5 )); then
    echo "Failed to parse https://felix86.com/releases.txt"
    return 1
  fi

  local entries=()
  while IFS= read -r line && [[ ${#entries[@]} -lt 5 ]]; do
    [[ "$line" =~ ^[0-9]+\.[0-9]+[[:space:]]+https?://.*$ ]] && entries+=("$line")
  done <<< "$url_list"

  if [[ ${#entries[@]} -eq 0 ]]; then
    echo "No valid entries found."
    return 1
  fi

  echo "┌───────────────────────────────────────┐"
  echo "│   Welcome to the felix86 installer!   │"
  echo "├───────────────────────────────────────┤"
  echo "│                                       │"
  echo "│  Please choose the version you'd      │"
  echo "│  like to install                      │"
  echo "│                                       │"
  echo "└───────────────────────────────────────┘"

  for i in "${!entries[@]}"; do
    local version="${entries[$i]%% *}"
    local display="felix86 $version"
    if [[ $i -eq 0 ]]; then
        display="$display (Default)"
    fi
    printf "%d) %s\n" $((i+1)) "$display"
  done

  local latest_index=$(( ${#entries[@]} + 1 ))
  echo "$latest_index) Nightly version (Unstable)"

  local choice
  while true; do
    read -p "Select a release (1-$latest_index) [default: 1]: " choice
    choice=${choice:-1}
    if [[ "$choice" =~ ^[1-9][0-9]*$ ]] && (( choice >= 1 && choice <= latest_index )); then
      break
    else
      echo "Invalid selection. Please choose a number between 1 and $latest_index."
    fi
  done

  if (( choice == latest_index )); then
    echo "Fetching latest artifact link..."
    FELIX86_LINK=$(curl -k -s "https://felix86.com/latest.txt")
  else
    selected="${entries[$((choice-1))]}"
    FELIX86_LINK="${selected#* }"
  fi
}

set -e

echo "Welcome to the felix86 installer"

select_release_url

echo "Downloading latest felix86 artifact..."
mkdir -p /tmp/felix86_artifact
curl -k -L $FELIX86_LINK -o /tmp/felix86_artifact/archive.zip
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

echo ""
echo "Which rootfs would you like to download?"
echo "1) Ubuntu 24.04 (Default)"
echo "2) I already have a rootfs"

while true; do
    read -p "Your choice [default: 1]: " choice
    choice=${choice:-1}
    if [[ "$choice" == "1" || "$choice" == "2" ]]; then
        break
    else
        echo "Invalid input. Please enter 1 or 2."
    fi
done


if [ "$choice" -eq 1 ]; then
    read -p "Installation path for rootfs [default: $INSTALLATION_DIR/rootfs]: " NEW_ROOTFS
    DEFAULT_ROOTFS=$INSTALLATION_DIR/rootfs
    NEW_ROOTFS=${NEW_ROOTFS:-$DEFAULT_ROOTFS}
    NEW_ROOTFS=$(eval echo "$NEW_ROOTFS")
    if [ ! -e "$NEW_ROOTFS" ] || [ -d "$NEW_ROOTFS" ] && [ -z "$(ls -A "$NEW_ROOTFS" 2> /dev/null)" ]; then
        echo "Downloading rootfs download link from felix86.com/rootfs/ubuntu.txt..."
        check_url "https://felix86.com/rootfs/ubuntu.txt"
        UBUNTU_2404_LINK=$(curl -k -s https://felix86.com/rootfs/ubuntu.txt)
        echo "Installing rootfs to $NEW_ROOTFS"
        echo "Creating rootfs directory..."
        sudo mkdir -p $NEW_ROOTFS
        echo "Downloading Ubuntu 24.04 rootfs..."
        check_url "$UBUNTU_2404_LINK"

        # Important we untar with --same-owner so that sudo/mount/fusermount keep their setuid bits
        curl -k -L $UBUNTU_2404_LINK | sudo tar --same-owner -xz -C $NEW_ROOTFS
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
    read -p "Path: " line
    felix86 --set-rootfs $line
fi

# Finally register felix86 in binfmt_misc
sudo -E felix86 --binfmt-misc

# Let's try to run a test program
set +e
FELIX86_QUIET=1 felix86 /bin/bash -c "exit 42"
exit_code=$?

# If everything went fine, felix86 should return 42
if [ "$exit_code" -ne 42 ]; then
    echo "felix86 failed to run a simple program -- may not have installed correctly?"
    echo "Running again with logging enabled:"
    felix86 /bin/bash -c "exit 42"
    exit 1
fi

echo "felix86 installed successfully"
echo
echo

echo "┌───────────────────────────────────────┐"
echo "│       felix86 is now installed!       │"
echo "├───────────────────────────────────────┤"
echo "│                                       │"
echo "│ To enter an emulated bash, use:       │"
echo "│                                       │"
echo "│ felix86 /bin/bash                     │"
echo "│                                       │"
echo "│   OR                                  │"
echo "│                                       │"
echo "│ /path/to/rootfs/bin/bash              │"
echo "│                                       │"
echo "├───────────────────────────────────────┤"
echo "│                                       │"
echo "│ Alternatively, run programs directly: │"
echo "│                                       │"
echo "│ /path/to/rootfs/MyGame.AppImage       │"
echo "│                                       │"
echo "└───────────────────────────────────────┘"

if ! felix86 -d; then
  echo "Failed binfmt_misc installation check"
  echo "felix86 is currently not used as the default emulator for x86 applications"
  echo "If you have other x86 emulators registered then you will need to unregister them"
  echo "Example of unregistering an emulator: sudo sh -c \"echo -1 > /proc/sys/fs/binfmt_misc/qemu-x86_64\""
  echo "To make this change permanent, make sure to remove the respective file from one of these directories:"
  echo "  /etc/binfmt.d"
  echo "  /run/binfmt.d"
  echo "  /usr/local/lib/binfmt.d"
  echo "  /usr/lib/binfmt.d"
  echo "After you're done, to register felix86, run: sudo -E $FILE -b"
  echo "If you don't register to binfmt_misc, some apps like sudo (needed by AppImages) will not work"
fi
