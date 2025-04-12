#include "utils.h"
#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <openssl/sha.h>

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

void log(const std::string& message) {
    std::ofstream logFile("ahmiyat.log", std::ios::app);
    logFile << "[" << time(nullptr) << "] " << message << std::endl;
}

std::string uploadToIPFS(const std::string& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        log("CURL initialization failed");
        return "";
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        log("Failed to open file for IPFS upload: " + filePath);
        curl_easy_cleanup(curl);
        return "";
    }

    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> fileData(fileSize);
    file.read(fileData.data(), fileSize);
    file.close();

    struct curl_httppost* formpost = NULL;
    struct curl_httppost* lastptr = NULL;
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "file",
                 CURLFORM_BUFFER, filePath.c_str(),
                 CURLFORM_BUFFERPTR, fileData.data(),
                 CURLFORM_BUFFERLENGTH, fileSize,
                 CURLFORM_END);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5001/api/v0/add");
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        log("IPFS upload failed: " + std::string(curl_easy_strerror(res)));
        curl_formfree(formpost);
        curl_easy_cleanup(curl);
        return "";
    }

    curl_formfree(formpost);
    curl_easy_cleanup(curl);

    size_t hashPos = response.find("\"Hash\":\"") + 8;
    size_t hashEnd = response.find("\"", hashPos);
    std::string ipfsHash = response.substr(hashPos, hashEnd - hashPos);
    log("Uploaded to IPFS: " + ipfsHash);
    return ipfsHash;
}

std::string generateZKProof(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}
