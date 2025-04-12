#include "dht.h"
#include <algorithm>

DHT::DHT() {}

void DHT::addPeer(const Node& node) {
    std::lock_guard<std::mutex> lock(dhtMutex);
    peers[node.nodeId] = node;
}

std::vector<Node> DHT::findPeers(const std::string& nodeId, int maxPeers) {
    std::lock_guard<std::mutex> lock(dhtMutex);
    std::vector<Node> closestPeers;
    for (const auto& [id, node] : peers) {
        if (id != nodeId) {
            closestPeers.push_back(node);
        }
    }
    if (closestPeers.size() > static_cast<size_t>(maxPeers)) {
        closestPeers.resize(maxPeers);
    }
    return closestPeers;
}

void DHT::bootstrap(const std::string& ip, int port) {
    bootstrapIp = ip;
    bootstrapPort = port;
    addPeer(Node("bootstrap", ip, port));
}
