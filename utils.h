#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <fstream> // Added for ofstream
#include <iomanip> // Added for setw, setfill

void log(const std::string& message);
std::string uploadToIPFS(const std::string& filePath);
std::string generateZKProof(const std::string& data);

#endif
