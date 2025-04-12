FROM ubuntu:20.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive

# Install all required libraries and tools
RUN apt-get update && apt-get install -y \
    g++ \
    libssl-dev \
    libleveldb-dev \
    libcurl4-openssl-dev \
    libmicrohttpd-dev \
    wget \
    tar \
    && rm -rf /var/lib/apt/lists/*

# Install IPFS using binary method
RUN wget https://dist.ipfs.io/go-ipfs/v0.20.0/go-ipfs_v0.20.0_linux-amd64.tar.gz \
    && tar -xvzf go-ipfs_v0.20.0_linux-amd64.tar.gz \
    && mv go-ipfs/ipfs /usr/local/bin/ \
    && rm -rf go-ipfs go-ipfs_v0.20.0_linux-amd64.tar.gz

# Set working directory
WORKDIR /app

# Copy all files
COPY . .

# Compile the code
RUN g++ -o ahmiyat blockchain.cpp dht.cpp wallet.cpp utils.cpp main.cpp -lssl -lcrypto -pthread -lleveldb -lcurl -lmicrohttpd -O3

# Expose ports
EXPOSE 5001 8080

# Start IPFS daemon and run the application
CMD ipfs init && ipfs daemon & sleep 5 && ./ahmiyat ${PORT:-5001}
