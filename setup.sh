#!/bin/bash
echo "Installing dependencies..."
sudo apt-get update
sudo apt-get install -y libssl-dev libleveldb-dev libcurl4-openssl-dev libmicrohttpd-dev nlohmann-json3-dev
if ! command -v ipfs &> /dev/null; then
    sudo snap install ipfs
fi
echo "Starting IPFS daemon..."
ipfs init 2>/dev/null
ipfs daemon &
echo "Dependencies installed!"
