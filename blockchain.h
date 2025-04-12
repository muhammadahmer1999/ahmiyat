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

namespace Ahmiyat {
    // Mainnet constants
    const int MAX_SHARDS = 16;
    const int INITIAL_DIFFICULTY = 4;
    const int TARGET_BLOCK_TIME = 60000; // 60 seconds in microseconds
}

struct Transaction {
    string sender;
    string receiver;
    double amount;
    double fee;
    string script;
    string signature;
    string shardId;
    uint64_t timestamp;
    Transaction(string s, string r, double a, double f = 0.001, string sc = "", string sh = "0");
    string toString() const;
    bool executeScript(const unordered_map<string, double>& balances);
    string getHash() const;
    bool validate() const; // New: Input validation
};

struct MemoryFragment {
    string type;
    string filePath;
    string ipfsHash;
    string description;
    string owner;
    int lockTime;
    MemoryFragment(string t, string fp, string desc, string o, int lt = 0);
    void saveToFile();
    bool validate() const; // New: Validate memory fragment
};

class AhmiyatBlock {
private:
    int index;
    uint64_t timestamp;
    vector<Transaction> transactions;
    MemoryFragment memory;
    string previousHash;
    string hash;
    int difficulty;
    string memoryProof;
    double stakeWeight;
    string shardId;
    string calculateHash() const;
    bool isMemoryProofValid(int difficulty);

public:
    AhmiyatBlock(int idx, const vector<Transaction>& txs, const MemoryFragment& mem, 
                 string prevHash, int diff, double stake, string sh);
    void mineBlock(double minerStake);
    string getHash() const;
    string getPreviousHash() const;
    string serialize() const;
    double getStakeWeight() const;
    string getShardId() const;
    const vector<Transaction>& getTransactions() const;
    bool validate() const; // New: Block validation
};

class ShardManager {
private:
    unordered_map<string, int> shardLoads;
    mutex loadMutex;
public:
    string assignShard(const Transaction& tx, int maxShards);
    void updateLoad(const string& shardId, int txCount);
};

class AhmiyatChain {
private:
    unordered_map<string, vector<AhmiyatBlock>> shards;
    unordered_map<string, unordered_map<string, double>> shardBalances;
    unordered_map<string, unordered_map<string, double>> shardStakes;
    unordered_map<string, int> shardDifficulties;
    vector<Node> nodes;
    DHT dht;
    mutex chainMutex;
    EC_KEY* keyPair;
    leveldb::DB* db;
    set<string> processedTxs;
    queue<Transaction> pendingTxs;
    ShardManager shardManager; // New: Dynamic shard management

    // Coin Economics
    const string COIN_NAME = "Ahmiyat Coin";
    const string COIN_SYMBOL = "AHM";
    const double MAX_SUPPLY = 21000000.0;
    double totalMined = 0.0;
    double blockReward = 50.0;
    const int HALVING_INTERVAL = 210000;
    double stakingReward = 0.1;
    unordered_map<string, pair<string, int>> governanceProposals;

    void broadcastBlock(const AhmiyatBlock& block, const Node& sender);
    string signTransaction(const Transaction& tx);
    void saveBlockToDB(const AhmiyatBlock& block);
    void loadChainFromDB();
    void syncChain(const string& blockData);
    void updateReward(string shardId);
    bool validateBlock(const AhmiyatBlock& block);
    void compressState(string shardId);
    string assignShard(const Transaction& tx);
    void processPendingTxs();

public:
    AhmiyatChain();
    ~AhmiyatChain();
    void addBlock(const vector<Transaction>& txs, const MemoryFragment& memory, string minerId, double stake);
    void addNode(string nodeId, string ip, int port);
    double getBalance(string address, string shardId = "0");
    void stakeCoins(string address, double amount, string shardId = "0");
    void adjustDifficulty(string shardId);
    void startNodeListener(int port);
    void stressTest(int numBlocks);
    void proposeUpgrade(string proposerId, string description);
    void voteForUpgrade(string voterId, string proposalId);
    string getShardStatus(string shardId);
    void handleCrossShardTx(const Transaction& tx);
    void addPendingTx(const Transaction& tx);
};

#endif
