#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <set>
#include <queue>
#include "wallet.h"
#include "dht.h"
#include <leveldb/db.h>

const int MAX_SHARDS = 16;
const int INITIAL_DIFFICULTY = 4;
const int TARGET_BLOCK_TIME = 60000;

struct Transaction {
    std::string sender;
    std::string receiver;
    double amount;
    double fee;
    std::string script;
    std::string signature;
    std::string shardId;
    uint64_t timestamp;
    Transaction(std::string s, std::string r, double a, double f = 0.001, std::string sh = "0");
    std::string toString() const;
    bool executeScript(const std::unordered_map<std::string, double>& balances);
    std::string getHash() const;
    bool validate() const;
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
    bool validate() const;
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
    std::string calculateHash() const;
    bool isMemoryProofValid(int difficulty);

public:
    AhmiyatBlock(int idx, const std::vector<Transaction>& txs, const MemoryFragment& mem, 
                 std::string prevHash, int diff, double stake, std::string sh);
    void mineBlock(double minerStake);
    std::string getHash() const;
    std::string getPreviousHash() const;
    std::string serialize() const;
    double getStakeWeight() const;
    std::string getShardId() const;
    const std::vector<Transaction>& getTransactions() const;
    bool validate() const;
};

class ShardManager {
private:
    std::unordered_map<std::string, int> shardLoads;
    std::mutex loadMutex;
public:
    std::string assignShard(const Transaction& tx, int maxShards);
    void updateLoad(const std::string& shardId, int txCount);
};

class AhmiyatChain {
private:
    std::unordered_map<std::string, std::vector<AhmiyatBlock>> shards;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> shardBalances;
    std::unordered_map<std::string, std::unordered_map<std::string, double>> shardStakes;
    std::unordered_map<std::string, int> shardDifficulties;
    std::vector<Node> nodes;
    DHT dht;
    std::mutex chainMutex;
    EC_KEY* keyPair;
    leveldb::DB* db;
    std::set<std::string> processedTxs;
    std::queue<Transaction> pendingTxs;
    ShardManager shardManager;

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
    void saveBlockToDB(const AhmiyatBlock& block);
    void loadChainFromDB();
    void syncChain(const std::string& blockData);
    void updateReward(std::string shardId);
    bool validateBlock(const AhmiyatBlock& block);
    void compressState(std::string shardId);
    std::string assignShard(const Transaction& tx);
    void processPendingTxs();

public:
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
