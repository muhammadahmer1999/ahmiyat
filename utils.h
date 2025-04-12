#ifndef UTILS_H
#define UTILS_H

#include <string>

void log(const std::string& message);
std::string uploadToStorj(const std::string& filePath);  // Changed from uploadToIPFS
std::string generateZKProof(const std::string& data);

#endif
