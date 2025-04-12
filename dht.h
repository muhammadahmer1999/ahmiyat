#ifndef DHT_H
#define DHT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

struct Node {
    std::string nodeId;
    std::string ip;
    int port;
    Node(std::string id, std::string ipAddr, int p) : nodeId(id), ip(ipAddr), port(p) {}
};

class DHT {
private:
    std::unordered_map<std::string, Node> peers;
    std::mutex dhtMutex;
    std::string bootstrapIp;
    int bootstrapPort;

public:
    DHT();
    void addPeer(const Node& node);
    std::vector<Node> findPeers(const std::string& nodeId, int maxPeers);
    void bootstrap(const std::string& ip, int port);
};

#endif
