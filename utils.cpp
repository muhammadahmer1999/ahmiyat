#include "utils.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <curl/curl.h>
#include <cstdlib>
#include <algorithm>

void log(const std::string& message) {
    std::ofstream logFile("simple_ahmiyat.log", std::ios::app);
    logFile << "[" << time(nullptr) << "] " << message << std::endl;
}

std::string uploadToStorj(const std::string& filePath) {
    log("Uploading file to Storj: " + filePath);

    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::string storjPath = "sj://ahmiyat-bucket/" + fileName;
    std::string command = "uplink cp " + filePath + " " + storjPath + " 2>&1";
    int sysResult = system(command.c_str());
    if (sysResult != 0) {
        log("System command failed for upload: " + command);
    }
    log("File uploaded to Storj: " + storjPath);

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

    result.erase(std::remove_if(result.begin(), result.end(), [](char c) { return c == '\n'; }), result.end());
    if (result.empty()) {
        log("Failed to retrieve shareable URL for " + storjPath);
        return storjPath;
    }

    log("File accessible at: " + result);
    return result;
}
