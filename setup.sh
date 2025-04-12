#!/bin/bash
echo "Updating package lists..."
sudo apt-get update -y

# Install software-properties-common for add-apt-repository
echo "Installing software-properties-common..."
sudo apt-get install -y software-properties-common

# Add additional repositories if needed
echo "Adding repositories for required packages..."
sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
sudo apt-get update -y

echo "Installing dependencies..."
# Install required dependencies
sudo apt-get install -y libssl-dev libcurl4-openssl-dev
sudo apt-get install -y libleveldb-dev || sudo apt-get install -y libsnappy-dev
sudo apt-get install -y libmicrohttpd-dev
sudo apt-get install -y nlohmann-json3-dev || sudo apt-get install -y libnlohmann-json-dev
sudo apt-get install -y unzip  # Added for unzip command

# Install Storj Uplink CLI
if ! command -v uplink &> /dev/null; then
    echo "Installing Storj Uplink CLI..."
    curl -L https://github.com/storj/storj/releases/latest/download/uplink_linux_amd64.zip -o uplink.zip
    unzip uplink.zip
    sudo mv uplink /usr/local/bin/uplink
    rm uplink.zip
    chmod +x /usr/local/bin/uplink
fi

# Setup Storj
echo "Setting up Storj..."
if [ -z "$STORJ_ACCESS_GRANT" ]; then
    echo "STORJ_ACCESS_GRANT environment variable not set. Please set it or run 'uplink setup' manually."
    echo "To get an access grant, sign up at https://storj.io, create a project, and generate an access key."
    exit 1
fi

# Import the access grant
uplink access import ahmiyat "$STORJ_ACCESS_GRANT"

# Create a bucket for storing files
echo "Creating Storj bucket..."
uplink mb sj://ahmiyat-bucket

# Share the bucket to ensure accessibility (optional, for read-only public access if needed)
uplink share --url --readonly sj://ahmiyat-bucket > /dev/null
if [ $? -eq 0 ]; then
    echo "Storj bucket 'ahmiyat-bucket' created and shared successfully!"
else
    echo "Failed to create or share Storj bucket. Please check manually."
    exit 1
fi

echo "Dependencies installed successfully!"
