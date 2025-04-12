#include "blockchain.h"
#include <openssl/sha.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <curl/curl.h>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex> // Added for lock_guard
#include <fstream>
#include <iomanip>

extern void log(const std::string& message);
extern std::string uploadToIPFS(const std::string& filePath);
extern std::string generateZKProof(const std::string& data);

Transaction::Transaction(std::string s, std::string r, double a, double f, std::string sc, std::string sh) 
    : sender(s), receiver(r), amount(a), fee(f), script(sc), shardId(sh), 
      timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}

std::string Transaction::toString() const {
    return sender + receiver + std::to_string(amount) + std::to_string(fee) + script + shardId + std::to_string(timestamp);
}

std::string Transaction::getHash() const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)toString().c_str(), toString().length(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool Transaction::executeScript(const std::unordered_map<std::string, double>& balances) {
    if (script.empty()) return true;
    if (script.find("BALANCE_CHECK") != std::string::npos) {
        double required = std::stod(script.substr(script.find("=") + 1));
        return balances.count(sender) && balances.at(sender) >= required;
    }
    return true;
}

MemoryFragment::MemoryFragment(std::string t, std::string fp, std::string desc, std::string o, int lt) 
    : type(t), filePath(fp), description(desc), owner(o), lockTime(lt) {
    saveToFile();
    ipfsHash = uploadToIPFS(filePath);
}

void MemoryFragment::saveToFile() {
    std::ofstream file(filePath, std::ios::binary);
    if (file.is_open()) {
        file << "Memory Data: " << description;
        file.close();
    } else {
        log("Error saving memory file: " + filePath);
    }
}

std::string AhmiyatBlock::calculateHash() const {
    std::stringstream ss;
    ss << index << timestamp;
    for (const auto& tx : transactions) {
        ss << tx.getHash();
    }
    ss << memory.ipfsHash << previousHash << memoryProof << stakeWeight << shardId;

    std::string input = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input.c_str(), input.length(), hash);

    std::stringstream hashStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hashStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return hashStream.str();
}

bool AhmiyatBlock::isMemoryProofValid(int difficulty) {
    std::string target(difficulty, '0');
    return hash.substr(0, difficulty) == target;
}

AhmiyatBlock::AhmiyatBlock(int idx, const std::vector<Transaction>& txs, const MemoryFragment& mem, 
                           std::string prevHash, int diff, double stake, std::string sh) 
    : index(idx), transactions(txs), memory(mem), previousHash(prevHash), difficulty(diff), 
      stakeWeight(stake), shardId(sh) {
    timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    mineBlock(stake);
}

void AhmiyatBlock::mineBlock(double minerStake) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    do {
        memoryProof = std::to_string(dis(gen));
        hash = calculateHash();
    } while (!isMemoryProofValid(difficulty) || (minerStake < stakeWeight && stakeWeight > 0));
    log("Block mined in shard " + shardId + " - Hash: " + hash.substr(0, 16));
}

std::string AhmiyatBlock::getHash() const { return hash; }
std::string AhmiyatBlock::getPreviousHash() const { return previousHash; }
double AhmiyatBlock::getStakeWeight() const { return stakeWeight; }
std::string AhmiyatBlock::getShardId() const { return shardId; }
const std::vector<Transaction>& AhmiyatBlock::getTransactions() const { return transactions; }

std::string AhmiyatBlock::serialize() const {
    std::stringstream ss;
    ss << index << "|" << timestamp << "|";
    for (const auto& tx : transactions) {
        ss << tx.sender << "," << tx.receiver << "," << tx.amount << "," << tx.fee << "," 
           << tx.signature << "," << tx.script << "," << tx.shardId << ";";
    }
    ss << "|" << memory.type << "," << memory.ipfsHash << "," << memory.description << ","
       << memory.owner << "," << memory.lockTime << "|" << previousHash << "|" << memoryProof 
       << "|" << stakeWeight << "|" << shardId;
    return ss.str();
}

AhmiyatChain::AhmiyatChain() {
    keyPair = EC_KEY_new_by_curve_name(NID_secp256k1);
    EC_KEY_generate_key(keyPair);

    leveldb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 64 * 1024 * 1024;
    leveldb::Status status = leveldb::DB::Open(options, "ahmiyat_db", &db);
    if (!status.ok()) {
        log("Failed to open LevelDB: " + status.ToString());
        exit(1);
    }

    if (shards["0"].empty()) {
        std::vector<Transaction> genesisTx = {Transaction("system", "genesis", 100.0)};
        genesisTx[0].signature = signTransaction(genesisTx[0]);
        MemoryFragment genesisMemory("text", "memories/genesis.txt", "The beginning of Ahmiyat", "system", 0);
        AhmiyatBlock genesisBlock(0, genesisTx, genesisMemory, "0", 1, 0.0, "0");
        shards["0"].push_back(genesisBlock);
        saveBlockToDB(genesisBlock);
        shardBalances["0"]["genesis"] = 100.0;
        shardStakes["0"]["genesis"] = 0.0;
        shardDifficulties["0"] = 1;
        totalMined += 100.0;
    }

    nodes.emplace_back("Node1", "node1.ahmiyat.example.com", 5001);
    nodes.emplace_back("Node2", "node2.ahmiyat.example.com", 5002);
    nodes.emplace_back("Node3", "node3.ahmiyat.example.com", 5003);
    for (const auto& node : nodes) dht.addPeer(node);
}

AhmiyatChain::~AhmiyatChain() {
    EC_KEY_free(keyPair);
    delete db;
}

void AhmiyatChain::broadcastBlock(const AhmiyatBlock& block, const Node& sender) {
    std::string blockData = block.serialize();
    std::vector<Node> peers = dht.findPeers(sender.nodeId);
    std::vector<std::thread> broadcastThreads;
    for (const auto& node : peers) {
        if (node.nodeId != sender.nodeId) {
            broadcastThreads.emplace_back([&, node]() {
                int retries = 3;
                while (retries--) {
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock < 0) {
                        log("Socket creation failed for " + node.nodeId);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    sockaddr_in addr;
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(node.port);
                    inet_pton(AF_INET, node.ip.c_str(), &addr.sin_addr);
                    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                        send(sock, blockData.c_str(), blockData.length(), 0);
                        log("Broadcast to " + node.nodeId + " in shard " + block.getShardId());
                        close(sock);
                        return;
                    }
                    log("Failed to connect to " + node.nodeId + ", retries left: " + std::to_string(retries));
                    close(sock);
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
            });
        }
    }
    for (auto& t : broadcastThreads) t.join();
}

std::string AhmiyatChain::signTransaction(const Transaction& tx) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)tx.toString().c_str(), tx.toString().length(), hash);

    unsigned char* signature = nullptr;
    unsigned int sigLen = 0;
    ECDSA_sign(0, hash, SHA256_DIGEST_LENGTH, signature, &sigLen, keyPair);

    std::stringstream sigStream;
    for (unsigned int i = 0; i < sigLen; i++) {
        sigStream << std::hex << std::setw(2) << std::setfill('0') << (int)signature[i];
    }
    free(signature);
    return sigStream.str();
}

bool AhmiyatChain::verifyTransaction(const Transaction& tx) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)tx.toString().c_str(), tx.toString().length(), hash);
    EC_KEY* pubKey = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (EC_KEY_oct2key(pubKey, (unsigned char*)tx.sender.c_str(), tx.sender.length(), nullptr) != 1) {
        log("Invalid public key for tx: " + tx.getHash());
        EC_KEY_free(pubKey);
        return false;
    }
    std::string sig = tx.signature;
    std::vector<unsigned char> sigBytes;
    for (size_t i = 0; i < sig.length(); i += 2) {
        sigBytes.push_back(std::stoi(sig.substr(i, 2), nullptr, 16));
    }
    int verified = ECDSA_verify(0, hash, SHA256_DIGEST_LENGTH, sigBytes.data(), sigBytes.size(), pubKey);
    EC_KEY_free(pubKey);
    return verified == 1;
}

void AhmiyatChain::saveBlockToDB(const AhmiyatBlock& block) {
    leveldb::WriteOptions options;
    options.sync = false;
    leveldb::Status status = db->Put(options, block.getHash(), block.serialize());
    if (!status.ok()) {
        log("Error saving block to DB: " + status.ToString());
    }
}

void AhmiyatChain::loadChainFromDB() {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        log("Loaded block from DB: " + it->key().ToString());
    }
    delete it;
}

void AhmiyatChain::syncChain(const std::string& blockData) {
    std::string shardId = blockData.substr(blockData.rfind("|") + 1);
    std::string hash = blockData.substr(blockData.rfind("|", blockData.rfind("|") - 1) + 1, 
                                      blockData.rfind("|") - blockData.rfind("|", blockData.rfind("|") - 1) - 1);
    std::lock_guard<std::mutex> lock(chainMutex);
    if (std::find_if(shards[shardId].begin(), shards[shardId].end(), 
                     [&](const AhmiyatBlock& b) { return b.getHash() == hash; }) == shards[shardId].end()) {
        log("Synced new block in shard " + shardId + ": " + hash);
    }
}

void AhmiyatChain::updateReward(std::string shardId) {
    if (shards[shardId].size() % HALVING_INTERVAL == 0 && shards[shardId].size() > 0) {
        blockReward /= 2;
        stakingReward *= 1.05;
        log("Shard " + shardId + ": Block reward halved to: " + std::to_string(blockReward));
    }
}

bool AhmiyatChain::validateBlock(const AhmiyatBlock& block) {
    std::string shardId = block.getShardId();
    std::lock_guard<std::mutex> lock(chainMutex);
    if (shards[shardId].empty() && block.getPreviousHash() != "0") return false;
    if (!shards[shardId].empty() && block.getPreviousHash() != shards[shardId].back().getHash()) return false;
    if (block.getHash() != block.calculateHash()) return false;
    for (const auto& tx : block.getTransactions()) {
        if (processedTxs.count(tx.signature)) return false;
    }
    return true;
}

void AhmiyatChain::compressState(std::string shardId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    std::stringstream ss;
    for (const auto& [addr, bal] : shardBalances[shardId]) {
        ss << addr << bal;
    }
    std::string proof = generateZKProof(ss.str());
    log("Shard " + shardId + " state compressed with ZKP: " + proof);
}

std::string AhmiyatChain::assignShard(const Transaction& tx) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)tx.sender.c_str(), tx.sender.length(), hash);
    return std::to_string(hash[0] % 8);
}

void AhmiyatChain::processPendingTxs() {
    std::lock_guard<std::mutex> lock(chainMutex);
    while (!pendingTxs.empty()) {
        Transaction tx = pendingTxs.front();
        pendingTxs.pop();
        std::vector<Transaction> txs = {tx};
        MemoryFragment mem("text", "memories/pending_" + tx.getHash() + ".txt", "Pending tx", tx.sender, 0);
        addBlock(txs, mem, tx.sender, shardStakes[tx.shardId][tx.sender]);
    }
}

void AhmiyatChain::addBlock(const std::vector<Transaction>& txs, const MemoryFragment& memory, std::string minerId, double stake) {
    std::lock_guard<std::mutex> lock(chainMutex);
    if (totalMined + blockReward > MAX_SUPPLY) {
        log("Max supply reached, no more mining rewards");
        return;
    }

    std::unordered_map<std::string, std::vector<Transaction>> shardTxs;
    for (auto tx : txs) {
        std::string shardId = assignShard(tx);
        tx.shardId = shardId;
        if (processedTxs.count(tx.signature)) continue;
        if (!verifyTransaction(tx)) {
            log("Invalid transaction signature for " + tx.getHash());
            continue;
        }
        tx.signature = signTransaction(tx);
        processedTxs.insert(tx.signature);
        shardTxs[shardId].push_back(tx);
    }

    std::vector<std::thread> blockThreads;
    for (auto& [shardId, txsInShard] : shardTxs) {
        blockThreads.emplace_back([&, shardId, txsInShard]() {
            AhmiyatBlock newBlock(shards[shardId].size(), txsInShard, memory, 
                                  shards[shardId].empty() ? "0" : shards[shardId].back().getHash(), 
                                  shardDifficulties[shardId], stake, shardId);
            if (!validateBlock(newBlock)) {
                log("Invalid block rejected in shard " + shardId);
                return;
            }
            {
                std::lock_guard<std::mutex> lock(chainMutex);
                shards[shardId].push_back(newBlock);
                saveBlockToDB(newBlock);
            }

            double totalFee = 0.0;
            for (const auto& tx : txsInShard) {
                if (!tx.executeScript(shardBalances[shardId])) continue;
                if (shardBalances[shardId][tx.sender] < tx.amount + tx.fee) {
                    log("Insufficient balance for " + tx.sender + " in shard " + shardId);
                    continue;
                    }
                shardBalances[shardId][tx.sender] -= (tx.amount + tx.fee);
                shardBalances[shardId][t
