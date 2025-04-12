#!/bin/bash
echo "Installing dependencies..."
sudo apt-get update
sudo apt-get install -y libssl-dev libcurl4-openssl-dev libleveldb-dev libmicrohttpd-dev nlohmann-json3-dev
echo "Dependencies installed!"
