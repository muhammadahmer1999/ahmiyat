#include "dht.h"
#include <openssl/sha.h>
#include <netdb.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>  // Added for setw and setfill

extern void log(const std::string& message);

Node::Node(std::string id, std::string ipAddr, int p) : nodeId(id), ip(ipAddr), port(p) {}

std::string DHT::hashNodeId(const std::string& nodeId) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)nodeId.c_str(), nodeId.length(), hash);
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

void DHT::addPeer(const Node& node) {
    std::lock_guard<std::mutex> lock(dhtMutex);
    peers[node.nodeId] = node;
    log("Added peer: " + node.nodeId);
}

std::vector<Node> DHT::findPeers(const std::string& targetId, int maxPeers) {
    std::lock_guard<std::mutex> lock(dhtMutex);
    std::vector<std::pair<std::string, Node> > sortedPeers;
    std::string targetHash = hashNodeId(targetId);

    for (std::unordered_map<std::string, Node>::const_iterator it = peers.begin(); it != peers.end(); ++it) {
        std::string peerHash = hashNodeId(it->first);
        sortedPeers.push_back(std::make_pair(peerHash, it->second));
    }

    std::sort(sortedPeers.begin(), sortedPeers.end(),
              [targetHash](const std::pair<std::string, Node>& a, const std::pair<std::string, Node>& b) {
                  return std::abs(std::stoll(a.first, nullptr, 16) - std::stoll(targetHash, nullptr, 16)) <
                         std::abs(std::stoll(b.first, nullptr, 16) - std::stoll(targetHash, nullptr, 16));
              });

    std::vector<Node> closestPeers;
    for (int i = 0; i < std::min(maxPeers, (int)sortedPeers.size()); i++) {
        closestPeers.push_back(sortedPeers[i].second);
    }
    return closestPeers;
}

void DHT::bootstrap(const std::string& bootstrapIp, int bootstrapPort) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log("Bootstrap socket creation failed");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bootstrapPort);
    inet_pton(AF_INET, bootstrapIp.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Bootstrap connection failed to " + bootstrapIp);
        close(sock);
        return;
    }

    char buffer[1024] = {0};
    ssize_t bytesRead = read(sock, buffer, 1024);
    if (bytesRead < 0) {
        log("Failed to read from bootstrap socket");
    } else {
        log("Bootstrapped with: " + std::string(buffer));
    }
    close(sock);
}

bool DHT::punchHole(const std::string& targetIp, int targetPort) {
    struct addrinfo hints, *res;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log("UDP socket creation failed");
        return false;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(targetIp.c_str(), std::to_string(targetPort).c_str(), &hints, &res) != 0) {
        log("Failed to resolve target address: " + targetIp);
        close(sock);
        return false;
    }

    char packet[] = "PUNCH";
    if (sendto(sock, packet, strlen(packet), 0, res->ai_addr, res->ai_addrlen) < 0) {
        log("Failed to send punch packet to " + targetIp);
        freeaddrinfo(res);
        close(sock);
        return false;
    }

    freeaddrinfo(res);
    close(sock);
    log("Punch hole successful to " + targetIp + ":" + std::to_string(targetPort));
    return true;
}
