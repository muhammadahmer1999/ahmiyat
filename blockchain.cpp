#include "blockchain.h"
#include "utils.h"
#include <openssl/sha.h>
#include <chrono>
#include <sstream>
#include <iomanip>

extern void log(const std::string& message);

Transaction::Transaction(std::string s, std::string r, double a) 
    : sender(s), receiver(r), amount(a), timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}

std::string Transaction::toString() const {
    return sender + receiver + std::to_string(amount) + std::to_string(timestamp);
}

std::string Transaction::getHash() const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)toString().c_str(), toString().length(), hash);
    std::ostringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

Block::Block(int idx, const std::vector<Transaction>& txs, std::string prevHash) 
    : index(idx), transactions(txs), previousHash(prevHash) {
    timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    hash = calculateHash();
}

std::string Block::calculateHash() const {
    std::ostringstream ss;
    ss << index << timestamp;
    for (const auto& tx : transactions) {
        ss << tx.getHash();
    }
    ss << previousHash;

    std::string input = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input.c_str(), input.length(), hash);

    std::ostringstream hashStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hashStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return hashStream.str();
}

std::string Block::serialize() const {
    std::ostringstream ss;
    ss << index << "|" << timestamp << "|";
    for (const auto& tx : transactions) {
        ss << tx.sender << "," << tx.receiver << "," << tx.amount << "," << tx.signature << ";";
    }
    ss << "|" << previousHash;
    return ss.str();
}

Blockchain::Blockchain() {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, "simple_ahmiyat_db", &db);
    if (!status.ok()) {
        log("Failed to open LevelDB: " + status.ToString());
        exit(1);
    }

    if (chain.empty()) {
        std::vector<Transaction> genesisTx = {Transaction("system", "genesis", 100.0)};
        genesisTx[0].signature = signTransaction(genesisTx[0]);
        Block genesisBlock(0, genesisTx, "0");
        chain.push_back(genesisBlock);
        saveBlockToDB(genesisBlock);
        balances["genesis"] = 100.0;
    }
}

Blockchain::~Blockchain() {
    delete db;
}

std::string Blockchain::signTransaction(const Transaction& tx) {
    std::string data = tx.toString();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    std::ostringstream sigStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sigStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return sigStream.str();
}

bool Blockchain::verifyTransaction(const Transaction& tx) {
    std::string computedSig = signTransaction(tx);
    return computedSig == tx.signature;
}

void Blockchain::saveBlockToDB(const Block& block) {
    leveldb::WriteOptions options;
    options.sync = false;
    leveldb::Status status = db->Put(options, block.getHash(), block.serialize());
    if (!status.ok()) {
        log("Error saving block to DB: " + status.ToString());
    }
}

void Blockchain::addBlock(const std::vector<Transaction>& txs) {
    std::lock_guard<std::mutex> lock(chainMutex);
    std::vector<Transaction> validTxs;
    for (auto tx : txs) {
        if (!verifyTransaction(tx)) {
            log("Invalid transaction signature for " + tx.getHash());
            continue;
        }
        if (balances[tx.sender] < tx.amount) {
            log("Insufficient balance for " + tx.sender);
            continue;
        }
        validTxs.push_back(tx);
    }

    if (validTxs.empty()) return;

    Block newBlock(chain.size(), validTxs, chain.back().hash);
    for (const auto& tx : validTxs) {
        balances[tx.sender] -= tx.amount;
        balances[tx.receiver] += tx.amount;
    }
    chain.push_back(newBlock);
    saveBlockToDB(newBlock);
    log("Block added: " + newBlock.hash);
}

double Blockchain::getBalance(const std::string& address) {
    std::lock_guard<std::mutex> lock(chainMutex);
    return balances[address];
}

void Blockchain::addPendingTx(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(chainMutex);
    pendingTxs.push(tx);
    log("Added pending tx: " + tx.getHash());
}

void Blockchain::processPendingTxs() {
    std::lock_guard<std::mutex> lock(chainMutex);
    while (!pendingTxs.empty()) {
        Transaction tx = pendingTxs.front();
        pendingTxs.pop();
        tx.signature = signTransaction(tx);
        addBlock({tx});
    }
}
