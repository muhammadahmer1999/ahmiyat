#include "utils.h"
#include <fstream>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <filesystem>

void log(const string& message) {
    static const size_t MAX_LOG_SIZE = 10 * 1024 * 1024; // 10MB
    string logFile = "ahmiyat.log";
    if (filesystem::exists(logFile) && filesystem::file_size(logFile) > MAX_LOG_SIZE) {
        filesystem::rename(logFile, logFile + ".bak");
    }
    ofstream file(logFile, ios::app);
    file << "[" << time(nullptr) << "] " << message << endl;
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string uploadToIPFS(const string& filePath) {
    const int retries = 3;
    for (int i = 0; i < retries; i++) {
        try {
            CURL* curl = curl_easy_init();
            if (!curl) throw runtime_error("CURL init failed");

            string response;
            CURLcode res;
            curl_mime* mime = curl_mime_init(curl);
            curl_mimepart* part = curl_mime_addpart(mime);
            curl_mime_name(part, "file");
            curl_mime_filedata(part, filePath.c_str());

            curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:5001/api/v0/add");
            curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            res = curl_easy_perform(curl);
            curl_mime_free(mime);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                throw runtime_error(curl_easy_strerror(res));
            }

            size_t pos = response.find("\"Hash\":\"") + 8;
            size_t end = response.find("\"", pos);
            return response.substr(pos, end - pos);
        } catch (const exception& e) {
            log("IPFS upload attempt " + to_string(i + 1) + " failed: " + e.what());
            this_thread::sleep_for(chrono::seconds(1));
        }
    }
    log("IPFS upload failed after " + to_string(retries) + " attempts");
    return "ERROR";
}

string generateZKProof(const string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return "ZKP_" + ss.str().substr(0, 16);
}
