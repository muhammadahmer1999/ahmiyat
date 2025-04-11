FROM ubuntu:20.04
# Install all required libraries
RUN apt-get update && apt-get install -y \
    g++ \
    libssl-dev \
    libleveldb-dev \
    libcurl4-openssl-dev \
    libmicrohttpd-dev \
    snapd \
    && snap install ipfs
# Set working directory
WORKDIR /app
# Copy all files
COPY . .
# Compile the code
RUN g++ -o ahmiyat blockchain.cpp dht.cpp wallet.cpp utils.cpp main.cpp -lssl -lcrypto -pthread -lleveldb -lcurl -lmicrohttpd -O3
# Dynamic port from environment variable
CMD ["sh", "-c", "./ahmiyat $PORT"]
