#ifndef WALLET_H
#define WALLET_H

#include <string>

class Wallet {
public:
    std::string publicKey;
    std::string privateKey;
    Wallet();
};

#endif
