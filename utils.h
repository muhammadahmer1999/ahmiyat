#ifndef UTILS_H
#define UTILS_H

#include <string>

void log(const std::string& message);
std::string uploadToIPFS(const std::string& filePath);
std::string generateZKProof(const std::string& data);

#endif
