#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <leveldb/db.h>

class Transaction {
public:
    std::string sender;
    std::string receiver;
    double amount;
    std::string signature;
    uint64_t timestamp;

    Transaction(std::string s, std::string r, double a);
    std::string toString() const;
    std::string getHash() const;
};

class Block {
public:
    int index;
    std::vector<Transaction> transactions;
    std::string previousHash;
    std::string hash;
    uint64_t timestamp;

    Block(int idx, const std::vector<Transaction>& txs, std::string prevHash);
    std::string calculateHash() const;
    std::string serialize() const;
    std::string getHash() const { return hash; }
    std::string getPreviousHash() const { return previousHash; }
};

class Blockchain {
public:
    std::vector<Block> chain;
    std::unordered_map<std::string, double> balances;
    std::queue<Transaction> pendingTxs;
    leveldb::DB* db;
    std::mutex chainMutex;

    Blockchain();
    ~Blockchain();

    void addBlock(const std::vector<Transaction>& txs);
    double getBalance(const std::string& address);
    void addPendingTx(const Transaction& tx);
    void processPendingTxs();
    void saveBlockToDB(const Block& block);
    std::string signTransaction(const Transaction& tx);
    bool verifyTransaction(const Transaction& tx);
};

#endif
