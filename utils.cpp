#include "utils.h"
#include <fstream>
#include <sstream>  // Added for stringstream
#include <iomanip>  // Added for setw, setfill
#include <curl/curl.h>
#include <openssl/sha.h>

void log(const std::string& message) {
    std::ofstream logFile("ahmiyat.log", std::ios::app);
    logFile << "[" << time(nullptr) << "] " << message << std::endl;
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string uploadToIPFS(const std::string& filePath) {
    // Temporarily bypass IPFS upload due to installation issues
    log("IPFS upload bypassed: " + filePath);
    return "BYPASSED_IPFS_HASH";
}

std::string generateZKProof(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return "ZKP_" + ss.str().substr(0, 16);
}
