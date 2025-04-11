#ifndef DHT_H
#define DHT_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>

struct Node {
    string nodeId;
    string ip;
    int port;
    Node(string id, string ipAddr, int p);
};

class DHT {
private:
    unordered_map<string, Node> peers;
    mutex dhtMutex;
    string hashNodeId(const string& nodeId);

public:
    void addPeer(const Node& node);
    vector<Node> findPeers(const string& targetId, int maxPeers = 10); // Optimized
    void bootstrap(const string& bootstrapIp, int bootstrapPort);
    bool punchHole(const string& targetIp, int targetPort);
};

#endif
