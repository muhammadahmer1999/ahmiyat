#ifndef UTILS_H
#define UTILS_H

#include <string>

void log(const string& message);
string uploadToIPFS(const string& filePath);
string generateZKProof(const string& data);

#endif
