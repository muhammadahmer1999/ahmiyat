#ifndef DHT_H
#define DHT_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

struct Node {
    std::string nodeId;
    std::string ip;
    int port;
    Node(std::string id, std::string ipAddr, int p);
};

class DHT {
private:
    std::unordered_map<std::string, Node> peers;
    std::mutex dhtMutex;
    std::string hashNodeId(const std::string& nodeId);

public:
    void addPeer(const Node& node);
    std::vector<Node> findPeers(const std::string& targetId, int maxPeers = 10);
    void bootstrap(const std::string& bootstrapIp, int bootstrapPort);
    bool punchHole(const std::string& targetIp, int targetPort);
};

#endif
