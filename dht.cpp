#include "dht.h"
#include <openssl/sha.h>
#include <arpa/inet.h>
#include <sys/socket.h>

extern void log(const string& message);

Node::Node(string id, string ipAddr, int p) : nodeId(id), ip(ipAddr), port(p) {}

string DHT::hashNodeId(const string& nodeId) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)nodeId.c_str(), nodeId.length(), hash);
    return string((char*)hash, SHA256_DIGEST_LENGTH);
}

void DHT::addPeer(const Node& node) {
    lock_guard<mutex> lock(dhtMutex);
    peers[hashNodeId(node.nodeId)] = node;
    log("Peer added to DHT: " + node.nodeId);
}

vector<Node> DHT::findPeers(const string& targetId, int maxPeers) {
    lock_guard<mutex> lock(dhtMutex);
    vector<pair<string, Node>> sortedPeers;
    string targetHash = hashNodeId(targetId);
    for (const auto& peer : peers) {
        sortedPeers.push_back({peer.first, peer.second});
    }
    sort(sortedPeers.begin(), sortedPeers.end(), 
         [&](const auto& a, const auto& b) { 
             return memcmp(a.first.c_str(), targetHash.c_str(), SHA256_DIGEST_LENGTH) < 
                    memcmp(b.first.c_str(), targetHash.c_str(), SHA256_DIGEST_LENGTH); 
         });
    vector<Node> closest;
    for (int i = 0; i < min(maxPeers, (int)sortedPeers.size()); i++) {
        closest.push_back(sortedPeers[i].second);
    }
    return closest;
}

void DHT::bootstrap(const string& bootstrapIp, int bootstrapPort) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(bootstrapPort);
    inet_pton(AF_INET, bootstrapIp.c_str(), &addr.sin_addr);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
        string msg = "BOOTSTRAP_REQUEST";
        send(sock, msg.c_str(), msg.length(), 0);
        char buffer[1024] = {0};
        read(sock, buffer, 1024);
        log("Bootstrapped with peers: " + string(buffer));
    }
    close(sock);
}

bool DHT::punchHole(const string& targetIp, int targetPort) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return false;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(targetPort);
    inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr);

    string msg = "PUNCH";
    sendto(sock, msg.c_str(), msg.length(), 0, (sockaddr*)&addr, sizeof(addr));
    log("Hole punched to " + targetIp + ":" + to_string(targetPort));
    close(sock);
    return true;
}
