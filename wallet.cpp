#include "wallet.h"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>

Wallet::Wallet() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::string key;
    for (int i = 0; i < 32; i++) {
        key += (char)dis(gen);
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)key.c_str(), key.length(), hash);

    std::ostringstream pub, priv;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        pub << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
        priv << std::hex << std::setw(2) << std::setfill('0') << (int)key[i % key.length()];
    }

    publicKey = pub.str();
    privateKey = priv.str();
}
