#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <set>
#include <queue>
#include <sstream>
#include <thread>
#include <openssl/ec.h>      // Added for EC_KEY
#include <openssl/ecdsa.h>   // Added for ECDSA
#include <openssl/obj_mac.h> // Added for NID_secp256k1
#include "wallet.h"
#include "dht.h"
#include <leveldb/db.h>

struct Transaction {
    std::string sender;
    std::string receiver;
    double amount;
    double fee;
    std::string script;
    std::string signature;
    std::string shardId;
    uint64_t timestamp;
    Transaction(std::string s, std::string r, double a, double f = 0.001, std::string sc = "", std::string sh = "0");
    std::string toString() const;
    bool executeScript(const std::unordered_map<std::string, double>& balances) const; // Made const
    std::string getHash() const;
};

struct MemoryFragment {
    std::string type;
    std::string filePath;
    std::string ipfsHash;
    std::string description;
    std::string owner;
    int lockTime;
    MemoryFragment(std::string t, std::string fp, std::string desc, std::string o, int lt = 0);
    void saveToFile();
};

class AhmiyatBlock {
private:
    int index;
    uint64_t timestamp;
    std::vector<Transaction> transactions;
    MemoryFragment memory;
    std::string previousHash;
    std::string hash;
    int difficulty;
    std::string memoryProof;
    double stakeWeight;
    std::string shardId;
    bool isMemoryProofValid(int difficulty);

public:
    AhmiyatBlock(int idx, const std::vector<Transaction>& txs, const MemoryFragment& mem, 
                 std::string prevHash, int diff, double stake, std::string sh);
    std::string calculateHash() const; // Made public
    void mineBlock(double minerStake);
    std::string getHash() const;
    std::string getPreviousHash() const;
    uint64_t getTimestamp() const; // Added getter for timestamp
    std::string serialize() const;
    double getStakeWeight() const;
    std::string getShardId() const;
    const std::vector<Transaction>& getTransactions() const;
};

class AhmiyatChain {
private:
    std::unordered_map<std::string, std::vector<AhmiyatBlock>> shards;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> shardBalances;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> shardStakes;
    std::unordered_map<std::string, int> shardDifficulties;
    std::vector<Node> nodes;
    std::mutex chainMutex;
    EC_KEY* keyPair;
    leveldb::DB* db;
    std::set<std::string> processedTxs;
    std::queue<Transaction> pendingTxs;

    const std::string COIN_NAME = "Ahmiyat Coin";
    const std::string COIN_SYMBOL = "AHM";
    const double MAX_SUPPLY = 21000000.0;
    double totalMined = 0.0;
    double blockReward = 50.0;
    const int HALVING_INTERVAL = 210000;
    double stakingReward = 0.1;
    std::unordered_map<std::string, std::pair<std::string, int>> governanceProposals;

    void broadcastBlock(const AhmiyatBlock& block, const Node& sender);
    std::string signTransaction(const Transaction& tx);
    bool verifyTransaction(const Transaction& tx);
    void saveBlockToDB(const AhmiyatBlock& block);
    void loadChainFromDB();
    void syncChain(const std::string& blockData);
    void updateReward(std::string shardId);
    bool validateBlock(const AhmiyatBlock& block);
    void compressState(std::string shardId);
    std::string assignShard(const Transaction& tx);
    void processPendingTxs();

public:
    DHT dht; // Made public
    AhmiyatChain();
    ~AhmiyatChain();
    void addBlock(const std::vector<Transaction>& txs, const MemoryFragment& memory, std::string minerId, double stake);
    void addNode(std::string nodeId, std::string ip, int port);
    double getBalance(std::string address, std::string shardId = "0");
    void stakeCoins(std::string address, double amount, std::string shardId = "0");
    void adjustDifficulty(std::string shardId);
    void startNodeListener(int port);
    void stressTest(int numBlocks);
    void proposeUpgrade(std::string proposerId, std::string description);
    void voteForUpgrade(std::string voterId, std::string proposalId);
    std::string getShardStatus(std::string shardId);
    void handleCrossShardTx(const Transaction& tx);
    void addPendingTx(const Transaction& tx);
};

#endif
