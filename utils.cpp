#include "utils.h"
#include <fstream>
#include <curl/curl.h>
#include <openssl/sha.h>

void log(const string& message) {
    ofstream logFile("ahmiyat.log", ios::app);
    logFile << "[" << time(nullptr) << "] " << message << endl;
}

size_t writeCallback(void* contents, size_t size, size_t nmemb, string* data) {
    data->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string uploadToIPFS(const string& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) return "ERROR";

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
    if (res != CURLE_OK) {
        log("IPFS upload failed: " + string(curl_easy_strerror(res)));
        return "ERROR";
    }

    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    size_t pos = response.find("\"Hash\":\"") + 8;
    size_t end = response.find("\"", pos);
    return response.substr(pos, end - pos);
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
