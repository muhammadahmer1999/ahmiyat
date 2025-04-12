#!/bin/bash
echo "Updating package lists..."
sudo apt-get update -y

# Add additional repositories if needed
echo "Adding repositories for required packages..."
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt-get update -y

echo "Installing dependencies..."
# Install required packages
sudo apt-get install -y libssl-dev libcurl4-openssl-dev
# Install leveldb (with fallback)
sudo apt-get install -y libleveldb-dev || sudo apt-get install -y libsnappy-dev
# Install microhttpd
sudo apt-get install -y libmicrohttpd-dev
# Install nlohmann-json (with fallback)
sudo apt-get install -y nlohmann-json3-dev || sudo apt-get install -y libnlohmann-json-dev

# Install snapd for IPFS installation
echo "Installing snapd for IPFS..."
sudo apt-get install -y snapd
sudo systemctl enable --now snapd.socket
sudo ln -s /var/lib/snapd/snap /snap 2>/dev/null || true

# Install IPFS using snap
echo "Installing IPFS via snap..."
sudo snap install ipfs || {
    echo "Failed to install IPFS via snap. Proceeding without IPFS..."
    echo "Note: IPFS is optional for core blockchain functionality. You can manually install it later."
    echo "Dependencies installed successfully (without IPFS)!"
    exit 0
}

# Verify IPFS installation
if ! command -v ipfs &> /dev/null; then
    echo "IPFS installation failed. Proceeding without IPFS..."
    echo "Note: IPFS is optional for core blockchain functionality. You can manually install it later."
    echo "Dependencies installed successfully (without IPFS)!"
    exit 0
fi
echo "IPFS installed successfully!"

# Initialize and start IPFS daemon
echo "Starting IPFS daemon..."
ipfs init 2>/dev/null
# Check if IPFS daemon is already running, if not, start it
if ! pgrep -x "ipfs" > /dev/null; then
    ipfs daemon &
    sleep 5 # Wait for daemon to start
    if pgrep -x "ipfs" > /dev/null; then
        echo "IPFS daemon started successfully!"
    else
        echo "Failed to start IPFS daemon. Proceeding without IPFS..."
        echo "Note: IPFS is optional for core blockchain functionality. You can manually install it later."
        echo "Dependencies installed successfully (without IPFS)!"
        exit 0
    fi
else
    echo "IPFS daemon is already running!"
fi

echo "Dependencies installed successfully!"
