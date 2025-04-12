#include "blockchain.h"
#include "utils.h"  // Added to include uploadToStorj
#include <openssl/sha.h>
#include <curl/curl.h>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <fstream>
#include <iomanip>
#include <sstream>

extern void log(const std::string& message);

Transaction::Transaction(std::string s, std::string r, double a, double f, std::string sc, std::string sh) 
    : sender(s), receiver(r), amount(a), fee(f), script(sc), shardId(sh), 
      timestamp(std::chrono::system_clock::now().time_since_epoch().count()) {}

std::string Transaction::toString() const {
    return sender + receiver + std::to_string(amount) + std::to_string(fee) + script + shardId + std::to_string(timestamp);
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

bool Transaction::executeScript(const std::unordered_map<std::string, double>& balances) const {
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
    fileURL = uploadToStorj(filePath);  // Changed from uploadToIPFS
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
    std::ostringstream ss;
    ss << index << timestamp;
    for (const auto& tx : transactions) {
        ss << tx.getHash();
    }
    ss << memory.fileURL << previousHash << memoryProof << stakeWeight << shardId;  // Changed from ipfsHash

    std::string input = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input.c_str(), input.length(), hash);

    std::ostringstream hashStream;
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
uint64_t AhmiyatBlock::getTimestamp() const { return timestamp; }
double AhmiyatBlock::getStakeWeight() const { return stakeWeight; }
std::string AhmiyatBlock::getShardId() const { return shardId; }
const std::vector<Transaction>& AhmiyatBlock::getTransactions() const { return transactions; }

std::string AhmiyatBlock::serialize() const {
    std::ostringstream ss;
    ss << index << "|" << timestamp << "|";
    for (const auto& tx : transactions) {
        ss << tx.sender << "," << tx.receiver << "," << tx.amount << "," << tx.fee << "," 
           << tx.signature << "," << tx.script << "," << tx.shardId << ";";
    }
    ss << "|" << memory.type << "," << memory.fileURL << "," << memory.description << ","  // Changed from ipfsHash
       << memory.owner << "," << memory.lockTime << "|" << previousHash << "|" << memoryProof 
       << "|" << stakeWeight << "|" << shardId;
    return ss.str();
}

AhmiyatChain::AhmiyatChain() {
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
    delete db;
}

void AhmiyatChain::broadcastBlock(const AhmiyatBlock& block, const Node& sender) {
    std::string blockData = block.serialize();
    std::vector<Node> peers = dht.findPeers(sender.nodeId);
    std::vector<std::thread> broadcastThreads;
    for (const auto& node : peers) {
        if (node.nodeId != sender.nodeId) {
            broadcastThreads.emplace_back([this, blockData, node]() {
                int retries = 3;
                while (retries--) {
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock < 0) {
                        log("Socket creation failed for " + node.nodeId);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        continue;
                    }
                    struct sockaddr_in addr;
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(node.port);
                    inet_pton(AF_INET, node.ip.c_str(), &addr.sin_addr);
                    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
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
    std::string data = tx.toString();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.length(), hash);
    std::ostringstream sigStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sigStream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return sigStream.str();
}

bool AhmiyatChain::verifyTransaction(const Transaction& tx) {
    std::string computedSig = signTransaction(tx);
    return computedSig == tx.signature;
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
    std::ostringstream ss;
    for (std::unordered_map<std::string, double>::const_iterator it = shardBalances[shardId].begin(); 
         it != shardBalances[shardId].end(); ++it) {
        ss << it->first << it->second;
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
    for (std::vector<Transaction>::const_iterator it = txs.begin(); it != txs.end(); ++it) {
        Transaction tx = *it;
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
    for (std::unordered_map<std::string, std::vector<Transaction> >::iterator it = shardTxs.begin(); 
         it != shardTxs.end(); ++it) {
        std::string shardId = it->first;
        std::vector<Transaction>& txsInShard = it->second;
        blockThreads.emplace_back([this, shardId, txsInShard, memory, minerId, stake]() {
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
                shardBalances[shardId][tx.receiver] += tx.amount;
                totalFee += tx.fee;
            }
            shardBalances[shardId][minerId] += blockReward + totalFee;
            if (stake > 0) shardBalances[shardId][minerId] += stakingReward;
            totalMined += blockReward;
            updateReward(shardId);
            broadcastBlock(newBlock, nodes[0]);
            compressState(shardId);
        });
    }
    for (std::vector<std::thread>::iterator t = blockThreads.begin(); t != blockThreads.end(); ++t) {
        t->join();
    }
}

void AhmiyatChain::addNode(std::string nodeId, std::string ip, int port) {
    std::lock_guard<std::mutex> lock(chainMutex);
    Node newNode(nodeId, ip, port);
    nodes.push_back(newNode);
    dht.addPeer(newNode);
}

double AhmiyatChain::getBalance(std::string address, std::string shardId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    return shardBalances[shardId][address];
}

void AhmiyatChain::stakeCoins(std::string address, double amount, std::string shardId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    if (shardBalances[shardId][address] >= amount) {
        shardBalances[shardId][address] -= amount;
        shardStakes[shardId][address] += amount;
        log(address + " staked " + std::to_string(amount) + " AHM in shard " + shardId);
    }
}

void AhmiyatChain::adjustDifficulty(std::string shardId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    if (shards[shardId].size() > 10) {
        uint64_t lastTenTime = shards[shardId].back().getTimestamp() - shards[shardId][shards[shardId].size() - 10].getTimestamp();
        double avgStake = 0;
        for (std::vector<AhmiyatBlock>::const_iterator it = shards[shardId].begin(); it != shards[shardId].end(); ++it) {
            avgStake += it->getStakeWeight();
        }
        avgStake /= shards[shardId].size();
        if (lastTenTime < 60000000 || avgStake > 1000) shardDifficulties[shardId]++;
        else if (lastTenTime > 120000000) shardDifficulties[shardId]--;
        log("Difficulty adjusted in shard " + shardId + " to: " + std::to_string(shardDifficulties[shardId]));
    }
}

void AhmiyatChain::startNodeListener(int port) {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        log("Socket creation failed");
        return;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    if (bind(serverSock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Bind failed on port " + std::to_string(port));
        close(serverSock);
        return;
    }
    listen(serverSock, 20);
    log("Node listening on port " + std::to_string(port));

    std::vector<std::thread> listenerThreads;
    while (true) {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) {
            log("Accept failed");
            continue;
        }
        listenerThreads.emplace_back([this, clientSock]() {
            char buffer[4096] = {0};
            if (read(clientSock, buffer, 4096) > 0) {
                std::string data(buffer);
                syncChain(data);
                processPendingTxs();
            }
            close(clientSock);
        });
        if (listenerThreads.size() > 100) {
            for (std::vector<std::thread>::iterator it = listenerThreads.begin(); it != listenerThreads.end();) {
                if (it->joinable()) {
                    it->join();
                    it = listenerThreads.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }
    close(serverSock);
}

void AhmiyatChain::stressTest(int numBlocks) {
    Wallet wallet;
    std::vector<std::thread> testThreads;
    for (int i = 0; i < numBlocks; i++) {
        testThreads.emplace_back([this, i, &wallet]() {
            std::vector<Transaction> txs = {Transaction(wallet.publicKey, "test" + std::to_string(i), 1.0)};
            MemoryFragment mem("text", "memories/test" + std::to_string(i) + ".txt", "Test block", wallet.publicKey, 0);
            addBlock(txs, mem, wallet.publicKey, shardStakes[assignShard(txs[0])][wallet.publicKey]);
        });
    }
    for (std::vector<std::thread>::iterator t = testThreads.begin(); t != testThreads.end(); ++t) {
        t->join();
    }
    log("Stress test completed: " + std::to_string(numBlocks) + " blocks added across shards");
}

void AhmiyatChain::proposeUpgrade(std::string proposerId, std::string description) {
    std::lock_guard<std::mutex> lock(chainMutex);
    std::string proposalId = proposerId + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    governanceProposals[proposalId] = std::make_pair(description, 0);
    log("Proposal " + proposalId + " submitted: " + description);
}

void AhmiyatChain::voteForUpgrade(std::string voterId, std::string proposalId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    for (std::unordered_map<std::string, std::unordered_map<std::string, double> >::const_iterator it = shardStakes.begin(); 
         it != shardStakes.end(); ++it) {
        const std::string& shardId = it->first;
        const std::unordered_map<std::string, double>& stakes = it->second;
        if (stakes.count(voterId)) {
            governanceProposals[proposalId].second += stakes.at(voterId);
            log(voterId + " voted for " + proposalId + " with " + std::to_string(stakes.at(voterId)) + " stake");
        }
    }
}

std::string AhmiyatChain::getShardStatus(std::string shardId) {
    std::lock_guard<std::mutex> lock(chainMutex);
    std::ostringstream ss;
    ss << "Shard " << shardId << ":\n";
    ss << "Blocks: " << shards[shardId].size() << "\n";
    ss << "Total Balance: ";
    double total = 0;
    for (std::unordered_map<std::string, double>::const_iterator it = shardBalances[shardId].begin(); 
         it != shardBalances[shardId].end(); ++it) {
        total += it->second;
    }
    ss << total << " AHM\n";
    ss << "Difficulty: " << shardDifficulties[shardId] << "\n";
    return ss.str();
}

void AhmiyatChain::handleCrossShardTx(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(chainMutex);
    std::string fromShard = tx.shardId;
    std::string toShard = assignShard(Transaction(tx.receiver, tx.sender, 0));
    if (fromShard != toShard) {
        if (shardBalances[fromShard][tx.sender] >= tx.amount + tx.fee) {
            shardBalances[fromShard][tx.sender] -= (tx.amount + tx.fee);
            shardBalances[toShard][tx.receiver] += tx.amount;
            log("Cross-shard tx from " + fromShard + " to " + toShard + ": " + std::to_string(tx.amount) + " AHM");
        }
    }
}

void AhmiyatChain::addPendingTx(const Transaction& tx) {
    std::lock_guard<std::mutex> lock(chainMutex);
    pendingTxs.push(tx);
    log("Added pending tx: " + tx.getHash());
}
