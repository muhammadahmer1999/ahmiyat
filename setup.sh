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

# Install IPFS using a more reliable method for Codespaces
echo "Installing IPFS..."
# Download IPFS binary
curl -L https://dist.ipfs.io/go-ipfs/v0.10.0/go-ipfs_v0.10.0_linux-amd64.tar.gz -o go-ipfs.tar.gz
tar -xvzf go-ipfs.tar.gz
cd go-ipfs
sudo mv ipfs /usr/local/bin/
cd ..
rm -rf go-ipfs go-ipfs.tar.gz

# Verify IPFS installation
if ! command -v ipfs &> /dev/null; then
    echo "IPFS installation failed. Exiting..."
    exit 1
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
        echo "Failed to start IPFS daemon. Please check manually."
        exit 1
    fi
else
    echo "IPFS daemon is already running!"
fi

echo "Dependencies installed successfully!"
