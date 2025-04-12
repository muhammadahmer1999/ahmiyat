#include "wallet.h"
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>

Wallet::Wallet() {
    EC_KEY* key = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_generate_key(key);

    const EC_POINT* pubKey = EC_KEY_get0_public_key(key);
    char* pubHex = EC_POINT_point2hex(EC_GROUP_new_by_curve_name(NID_secp256k1), 
                                      pubKey, POINT_CONVERSION_UNCOMPRESSED, nullptr);
    publicKey = std::string(pubHex);
    OPENSSL_free(pubHex);

    const BIGNUM* privKey = EC_KEY_get0_private_key(key);
    char* privHex = BN_bn2hex(privKey);
    privateKey = std::string(privHex);
    OPENSSL_free(privHex);

    EC_KEY_free(key);
}
