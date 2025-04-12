#include "blockchain.h"
#include <openssl/sha.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <curl/curl.h>
#include <random>
#include <algorithm>
#include <chrono>

extern void log(const string& message);
extern string uploadToIPFS(const string& filePath);
extern string generateZKProof(const string& data);

bool Transaction::validate() const {
    if (sender.empty() || receiver.empty() || sender == receiver) return false;
    if (amount < 0 || fee < 0 || amount > MAX_SUPPLY || fee > amount) return false;
    if (timestamp == 0 || shardId.empty()) return false;
    // Add signature verification (requires public key)
    return true;
}

bool MemoryFragment::validate() const {
    return !type.empty() && !filePath.empty() && !owner.empty() && lockTime >= 0;
}

Transaction::Transaction(string s, string r, double a, double f, string sc, string sh) 
    : sender(s), receiver(r), amount(a), fee(f), script(sc), shardId(sh), 
      timestamp(chrono::system_clock::now().time_since_epoch().count()) {
    if (!validate()) throw runtime_error("Invalid transaction");
}

string Transaction::toString() const {
    return sender + receiver + to_string(amount) + to_string(fee) + script + shardId + to_string(timestamp);
}

string Transaction::getHash() const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)toString().c_str(), toString().length(), hash);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

bool Transaction::executeScript(const unordered_map<string, double>& balances) {
    if (script.empty()) return true;
    if (script.find("BALANCE_CHECK") != string::npos) {
        try {
            double required = stod(script.substr(script.find("=") + 1));
            return balances.count(sender) && balances.at(sender) >= required;
        } catch (...) {
            return false;
        }
    }
    return true;
}

MemoryFragment::MemoryFragment(string t, string fp, string desc, string o, int lt) 
    : type(t), filePath(fp), description(desc), owner(o), lockTime(lt) {
    if (!validate()) throw runtime_error("Invalid memory fragment");
    saveToFile();
    ipfsHash = uploadToIPFS(filePath);
}

void MemoryFragment::saveToFile() {
    ofstream file(filePath, ios::binary);
    if (file.is_open()) {
        file << "Memory Data: " << description;
        file.close();
    } else {
        log("Error saving memory file: " + filePath);
        throw runtime_error("Failed to save memory file");
    }
}

string AhmiyatBlock::calculateHash() const {
    stringstream ss;
    ss << index << timestamp;
    for (const auto& tx : transactions) {
        ss << tx.getHash();
    }
    ss << memory.ipfsHash << previousHash << memoryProof << stakeWeight << shardId;

    string input = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)input.c_str(), input.length(), hash);

    stringstream hashStream;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        hashStream << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return hashStream.str();
}

bool AhmiyatBlock::isMemoryProofValid(int difficulty) {
    string target(difficulty, '0');
    return hash.substr(0, difficulty) == target;
}

bool AhmiyatBlock::validate() const {
    if (index < 0 || difficulty < 0 || stakeWeight < 0) return false;
    for (const auto& tx : transactions) {
        if (!tx.validate()) return false;
    }
    if (!memory.validate()) return false;
    return calculateHash() == hash && isMemoryProofValid(difficulty);
}

AhmiyatBlock::AhmiyatBlock(int idx, const vector<Transaction>& txs, const MemoryFragment& mem, 
                           string prevHash, int diff, double stake, string sh) 
    : index(idx), transactions(txs), memory(mem), previousHash(prevHash), difficulty(diff), 
      stakeWeight(stake), shardId(sh) {
    timestamp = chrono::system_clock::now().time_since_epoch().count();
    mineBlock(stake);
    if (!validate()) throw runtime_error("Invalid block created");
}

void AhmiyatBlock::mineBlock(double minerStake) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);

    int attempts = 0;
    const int maxAttempts = 1000000; // Prevent infinite loops
    do {
        memoryProof = to_string(dis(gen));
        hash = calculateHash();
        attempts++;
        if (attempts > maxAttempts) throw runtime_error("Mining failed: too many attempts");
    } while (!isMemoryProofValid(difficulty) || (minerStake < stakeWeight && stakeWeight > 0));
    log("Block mined in shard " + shardId + " - Hash: " + hash.substr(0, 16));
}

string AhmiyatBlock::getHash() const { return hash; }
string AhmiyatBlock::getPreviousHash() const { return previousHash; }
double AhmiyatBlock::getStakeWeight() const { return stakeWeight; }
string AhmiyatBlock::getShardId() const { return shardId; }
const vector<Transaction>& AhmiyatBlock::getTransactions() const { return transactions; }

string AhmiyatBlock::serialize() const {
    stringstream ss;
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

string ShardManager::assignShard(const Transaction& tx, int maxShards) {
    lock_guard<mutex> lock(loadMutex);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)tx.sender.c_str(), tx.sender.length(), hash);
    string shardId = to_string(hash[0] % maxShards);
    
    // Load balancing: Redirect to less loaded shard if current is overloaded
    if (shardLoads[shardId] > 1000) { // Arbitrary threshold
        for (int i = 0; i < maxShards; i++) {
            string altShard = to_string(i);
            if (shardLoads[altShard] < shardLoads[shardId]) {
                shardId = altShard;
                break;
            }
        }
    }
    return shardId;
}

void ShardManager::updateLoad(const string& shardId, int txCount) {
    lock_guard<mutex> lock(loadMutex);
    shardLoads[shardId] += txCount;
}

AhmiyatChain::AhmiyatChain() {
    keyPair = EC_KEY_new_by_curve_name(NID_secp256k1);
    if (!EC_KEY_generate_key(keyPair)) {
        log("Failed to generate ECDSA key pair");
        exit(1);
    }

    leveldb::Options options;
    options.create_if_missing = true;
    options.write_buffer_size = 64 * 1024 * 1024;
    options.compression = leveldb::kSnappyCompression; // Enable compression
    leveldb::Status status = leveldb::DB::Open(options, "ahmiyat_db", &db);
    if (!status.ok()) {
        log("Failed to open LevelDB: " + status.ToString());
        exit(1);
    }

    // Initialize shards
    shardDifficulties["0"] = Ahmiyat::INITIAL_DIFFICULTY;
    if (shards["0"].empty()) {
        vector<Transaction> genesisTx = {Transaction("system", "genesis", 100.0)};
        genesisTx[0].signature = signTransaction(genesisTx[0]);
        MemoryFragment genesisMemory("text", "memories/genesis.txt", "The beginning of Ahmiyat", "system", 0);
        AhmiyatBlock genesisBlock(0, genesisTx, genesisMemory, "0", Ahmiyat::INITIAL_DIFFICULTY, 0.0, "0");
        shards["0"].push_back(genesisBlock);
        saveBlockToDB(genesisBlock);
        shardBalances["0"]["genesis"] = 100.0;
        shardStakes["0"]["genesis"] = 0.0;
        totalMined += 100.0;
    }

    nodes.emplace_back("Node1", "127.0.0.1", 5001);
    nodes.emplace_back("Node2", "127.0.0.1", 5002);
    for (const auto& node : nodes) dht.addPeer(node);
}

AhmiyatChain::~AhmiyatChain() {
    leveldb::WriteOptions options;
    options.sync = true; // Ensure all data is flushed
    db->SyncWAL();
    EC_KEY_free(keyPair);
    delete db;
}

void AhmiyatChain::broadcastBlock(const AhmiyatBlock& block, const Node& sender) {
    string blockData = block.serialize();
    vector<Node> peers = dht.findPeers(sender.nodeId, 10);
    vector<thread> broadcastThreads;
    for (const auto& node : peers) {
        if (node.nodeId != sender.nodeId) {
            broadcastThreads.emplace_back([&, node]() {
                try {
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock < 0) throw runtime_error("Socket creation failed");

                    sockaddr_in addr;
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(node.port);
                    if (inet_pton(AF_INET, node.ip.c_str(), &addr.sin_addr) <= 0) {
                        throw runtime_error("Invalid IP address");
                    }

                    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                        send(sock, blockData.c_str(), blockData.length(), 0);
                        log("Broadcast to " + node.nodeId + " in shard " + block.getShardId());
                    }
                    close(sock);
                } catch (const exception& e) {
                    log("Broadcast failed to " + node.nodeId + ": " + e.what());
                }
            });
        }
    }
    for (auto& t : broadcastThreads) t.join();
}

string AhmiyatChain::signTransaction(const Transaction& tx) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)tx.toString().c_str(), tx.toString().length(), hash);

    unsigned char signature[1024];
    unsigned int sigLen = 0;
    if (!ECDSA_sign(0, hash, SHA256_DIGEST_LENGTH, signature, &sigLen, keyPair)) {
        throw runtime_error("Failed to sign transaction");
    }

    stringstream sigStream;
    for (unsigned int i = 0; i < sigLen; i++) {
        sigStream << hex << setw(2) << setfill('0') << (int)signature[i];
    }
    return sigStream.str();
}

void AhmiyatChain::saveBlockToDB(const AhmiyatBlock& block) {
    leveldb::WriteBatch batch;
    batch.Put(block.getHash(), block.serialize());
    leveldb::WriteOptions options;
    options.sync = false;
    leveldb::Status status = db->Write(options, &batch);
    if (!status.ok()) {
        log("Error saving block to DB: " + status.ToString());
        throw runtime_error("DB write failed");
    }
}

void AhmiyatChain::loadChainFromDB() {
    leveldb::Iterator* it = db->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        log("Loaded block from DB: " + it->key().ToString());
    }
    if (!it->status().ok()) {
        log("DB read error: " + it->status().ToString());
    }
    delete it;
}

void AhmiyatChain::syncChain(const string& blockData) {
    try {
        string shardId = blockData.substr(blockData.rfind("|") + 1);
        string hash = blockData.substr(blockData.rfind("|", blockData.rfind("|") - 1) + 1, 
                                      blockData.rfind("|") - blockData.rfind("|", blockData.rfind("|") - 1) - 1);
        lock_guard<mutex> lock(chainMutex);
        if (find_if(shards[shardId].begin(), shards[shardId].end(), 
                    [&](const AhmiyatBlock& b) { return b.getHash() == hash; }) == shards[shardId].end()) {
            // Deserialize and validate block (simplified)
            log("Synced new block in shard " + shardId + ": " + hash);
        }
    } catch (const exception& e) {
        log("Sync failed: " + string(e.what()));
    }
}

void AhmiyatChain::updateReward(string shardId) {
    lock_guard<mutex> lock(chainMutex);
    if (shards[shardId].size() % HALVING_INTERVAL == 0 && shards[shardId].size() > 0) {
        blockReward /= 2;
        stakingReward *= 1.05;
        log("Shard " + shardId + ": Block reward halved to: " + to_string(blockReward));
    }
}

bool AhmiyatChain::validateBlock(const AhmiyatBlock& block) {
    string shardId = block.getShardId();
    lock_guard<mutex> lock(chainMutex);
    if (shards[shardId].empty() && block.getPreviousHash() != "0") return false;
    if (!shards[shardId].empty() && block.getPreviousHash() != shards[shardId].back().getHash()) return false;
    if (!block.validate()) return false;
    for (const auto& tx : block.getTransactions()) {
        if (processedTxs.count(tx.signature)) return false;
        if (!tx.validate()) return false;
    }
    return true;
}

void AhmiyatChain::compressState(string shardId) {
    lock_guard<mutex> lock(chainMutex);
    stringstream ss;
    for (const auto& [addr, bal] : shardBalances[shardId]) {
        ss << addr << bal;
    }
    string proof = generateZKProof(ss.str());
    log("Shard " + shardId + " state compressed with ZKP: " + proof.substr(0, 16));
}

string AhmiyatChain::assignShard(const Transaction& tx) {
    return shardManager.assignShard(tx, Ahmiyat::MAX_SHARDS);
}

void AhmiyatChain::processPendingTxs() {
    vector<Transaction> batch;
    {
        lock_guard<mutex> lock(chainMutex);
        while (!pendingTxs.empty()) {
            batch.push_back(pendingTxs.front());
            pendingTxs.pop();
        }
    }
    for (const auto& tx : batch) {
        try {
            vector<Transaction> txs = {tx};
            MemoryFragment mem("text", "memories/pending_" + tx.getHash() + ".txt", "Pending tx", tx.sender, 0);
            addBlock(txs, mem, tx.sender, shardStakes[tx.shardId][tx.sender]);
        } catch (const exception& e) {
            log("Failed to process tx " + tx.getHash() + ": " + e.what());
        }
    }
}

void AhmiyatChain::addBlock(const vector<Transaction>& txs, const MemoryFragment& memory, string minerId, double stake) {
    if (totalMined + blockReward > MAX_SUPPLY) {
        log("Max supply reached, no more mining rewards");
        return;
    }

    unordered_map<string, vector<Transaction>> shardTxs;
    for (auto tx : txs) {
        try {
            if (!tx.validate()) continue;
            string shardId = assignShard(tx);
            tx.shardId = shardId;
            if (processedTxs.count(tx.signature)) continue;
            tx.signature = signTransaction(tx);
            processedTxs.insert(tx.signature);
            shardTxs[shardId].push_back(tx);
            shardManager.updateLoad(shardId, 1);
        } catch (const exception& e) {
            log("Invalid tx: " + string(e.what()));
        }
    }

    vector<thread> blockThreads;
    for (auto& [shardId, txsInShard] : shardTxs) {
        blockThreads.emplace_back([&, shardId, txsInShard]() {
            try {
                AhmiyatBlock newBlock(shards[shardId].size(), txsInShard, memory, 
                                      shards[shardId].empty() ? "0" : shards[shardId].back().getHash(), 
                                      shardDifficulties[shardId], stake, shardId);
                if (!validateBlock(newBlock)) {
                    log("Invalid block rejected in shard " + shardId);
                    return;
                }
                {
                    lock_guard<mutex> lock(chainMutex);
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
            } catch (const exception& e) {
                log("Block creation failed in shard " + shardId + ": " + e.what());
            }
        });
    }
    for (auto& t : blockThreads) t.join();
}

void AhmiyatChain::addNode(string nodeId, string ip, int port) {
    lock_guard<mutex> lock(chainMutex);
    if (nodeId.empty() || ip.empty() || port <= 0) {
        log("Invalid node parameters");
        return;
    }
    Node newNode(nodeId, ip, port);
    nodes.push_back(newNode);
    dht.addPeer(newNode);
}

double AhmiyatChain::getBalance(string address, string shardId) {
    lock_guard<mutex> lock(chainMutex);
    if (address.empty() || !shardBalances.count(shardId)) return 0.0;
    return shardBalances[shardId][address];
}

void AhmiyatChain::stakeCoins(string address, double amount, string shardId) {
    lock_guard<mutex> lock(chainMutex);
    if (amount <= 0 || address.empty() || !shardBalances.count(shardId)) return;
    if (shardBalances[shardId][address] >= amount) {
        shardBalances[shardId][address] -= amount;
        shardStakes[shardId][address] += amount;
        log(address + " staked " + to_string(amount) + " AHM in shard " + shardId);
    }
}

void AhmiyatChain::adjustDifficulty(string shardId) {
    lock_guard<mutex> lock(chainMutex);
    if (shards[shardId].size() <= 10) return;
    uint64_t lastTenTime = shards[shardId].back().timestamp - shards[shardId][shards[shardId].size() - 10].timestamp;
    double avgStake = 0;
    for (const auto& block : shards[shardId]) avgStake += block.getStakeWeight();
    avgStake /= shards[shardId].size();
    if (lastTenTime < Ahmiyat::TARGET_BLOCK_TIME || avgStake > 1000) {
        shardDifficulties[shardId]++;
    } else if (lastTenTime > 2 * Ahmiyat::TARGET_BLOCK_TIME) {
        shardDifficulties[shardId] = max(1, shardDifficulties[shardId] - 1);
    }
    log("Difficulty adjusted in shard " + shardId + " to: " + to_string(shardDifficulties[shardId]));
}

void AhmiyatChain::startNodeListener(int port) {
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        log("Socket creation failed");
        return;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        log("Bind failed on port " + to_string(port));
        close(serverSock);
        return;
    }
    listen(serverSock, 20);
    log("Node listening on port " + to_string(port));

    vector<thread> listenerThreads;
    while (true) {
        try {
            int clientSock = accept(serverSock, nullptr, nullptr);
            if (clientSock < 0) continue;

            listenerThreads.emplace_back([&, clientSock]() {
                try {
                    char buffer[4096] = {0};
                    ssize_t bytesRead = read(clientSock, buffer, 4096);
                    if (bytesRead > 0) {
                        string data(buffer, bytesRead);
                        syncChain(data);
                        processPendingTxs();
                    }
                    close(clientSock);
                } catch (const exception& e) {
                    log("Listener error: " + string(e.what()));
                    close(clientSock);
                }
            });
            // Cleanup threads
            for (auto it = listenerThreads.begin(); it != listenerThreads.end();) {
                if (it->joinable()) {
                    it->join();
                    it = listenerThreads.erase(it);
                } else {
                    ++it;
                }
            }
        } catch (const exception& e) {
            log("Accept error: " + e.what());
        }
    }
    close(serverSock);
}

void AhmiyatChain::stressTest(int numBlocks) {
    Wallet wallet;
    vector<thread> testThreads;
    for (int i = 0; i < numBlocks; i++) {
        testThreads.emplace_back([&, i]() {
            try {
                vector<Transaction> txs = {Transaction(wallet.publicKey, "test" + to_string(i), 1.0)};
                MemoryFragment mem("text", "memories/test" + to_string(i) + ".txt", "Test block", wallet.publicKey, 0);
                addBlock(txs, mem, wallet.publicKey, shardStakes[assignShard(txs[0])][wallet.publicKey]);
            } catch (const exception& e) {
                log("Stress test block " + to_string(i) + " failed: " + e.what());
            }
        });
    }
    for (auto& t : testThreads) t.join();
    log("Stress test completed: " + to_string(numBlocks) + " blocks added across shards");
}

void AhmiyatChain::proposeUpgrade(string proposerId, string description) {
    lock_guard<mutex> lock(chainMutex);
    if (proposerId.empty() || description.empty()) return;
    string proposalId = proposerId + to_string(chrono::system_clock::now().time_since_epoch().count());
    governanceProposals[proposalId] = {description, 0};
    log("Proposal " + proposalId + " submitted: " + description);
}

void AhmiyatChain::voteForUpgrade(string voterId, string proposalId) {
    lock_guard<mutex> lock(chainMutex);
    if (!governanceProposals.count(proposalId)) return;
    for (const auto& [shardId, stakes] : shardStakes) {
        if (stakes.count(voterId)) {
            governanceProposals[proposalId].second += stakes.at(voterId);
            log(voterId + " voted for " + proposalId + " with " + to_string(stakes.at(voterId)) + " stake");
        }
    }
}

string AhmiyatChain::getShardStatus(string shardId) {
    lock_guard<mutex> lock(chainMutex);
    if (!shards.count(shardId)) return "Shard not found";
    stringstream ss;
    ss << "Shard " << shardId << ":\n";
    ss << "Blocks: " << shards[shardId].size() << "\n";
    ss << "Total Balance: ";
    double total = 0;
    for (const auto& [addr, bal] : shardBalances[shardId]) total += bal;
    ss << total << " AHM\n";
    ss << "Difficulty: " << shardDifficulties[shardId] << "\n";
    return ss.str();
}

void AhmiyatChain::handleCrossShardTx(const Transaction& tx) {
    lock_guard<mutex> lock(chainMutex);
    if (!tx.validate()) return;
    string fromShard = tx.shardId;
    string toShard = assignShard(Transaction(tx.receiver, tx.sender, 0));
    if (fromShard != toShard) {
        if (shardBalances[fromShard][tx.sender] >= tx.amount + tx.fee) {
            shardBalances[fromShard][tx.sender] -= (tx.amount + tx.fee);
            shardBalances[toShard][tx.receiver] += tx.amount;
            log("Cross-shard tx from " + fromShard + " to " + toShard + ": " + to_string(tx.amount) + " AHM");
        } else {
            log("Cross-shard tx failed: insufficient balance");
        }
    }
}

void AhmiyatChain::addPendingTx(const Transaction& tx) {
    if (!tx.validate()) {
        log("Invalid pending tx rejected");
        return;
    }
    lock_guard<mutex> lock(chainMutex);
    pendingTxs.push(tx);
    log("Added pending tx: " + tx.getHash());
}
