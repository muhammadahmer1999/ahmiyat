#include "utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <cstdlib>  // For system()

void log(const std::string& message) {
    std::ofstream logFile("ahmiyat.log", std::ios::app);
    logFile << "[" << time(nullptr) << "] " << message << std::endl;
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string uploadToStorj(const std::string& filePath) {
    log("Uploading file to Storj: " + filePath);

    // Use uplink CLI to upload the file to Storj bucket
    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::string storjPath = "sj://ahmiyat-bucket/" + fileName;
    std::string command = "uplink cp " + filePath + " " + storjPath + " 2>&1";
    system(command.c_str());
    log("File uploaded to Storj: " + storjPath);

    // Generate a shareable URL for retrieval
    std::string shareCommand = "uplink share --url --readonly " + storjPath + " | grep URL | awk '{print $2}'";
    FILE* pipe = popen(shareCommand.c_str(), "r");
    if (!pipe) {
        log("Failed to get Storj URL for " + storjPath);
        return "STORJ_UPLOAD_FAILED";
    }

    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);

    // Clean up the result (remove trailing newline)
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    if (result.empty()) {
        log("Failed to retrieve shareable URL for " + storjPath);
        return storjPath;  // Fallback to the path if URL generation fails
    }

    log("File accessible at: " + result);
    return result;  // Return the shareable URL
}

std::string generateZKProof(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return "ZKP_" + ss.str().substr(0, 16);
}
