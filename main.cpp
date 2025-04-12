#include "blockchain.h"
#include <thread>
#include <iostream>
#include <microhttpd.h>
#include <csignal>
#include <fstream>
#include <sstream>
#include <chrono>

volatile sig_atomic_t keepRunning = 1;

void signalHandler(int sig) {
    keepRunning = 0;
}

void log(const std::string& message) {
    std::ofstream logFile("ahmiyat.log", std::ios::app);
    logFile << "[" << time(nullptr) << "] " << message << std::endl;
}

void runNode(AhmiyatChain& chain, int port) {
    chain.startNodeListener(port);
}

void mineBlock(AhmiyatChain& chain, std::string minerId) {
    Wallet wallet;
    std::vector<Transaction> txs = {Transaction(wallet.publicKey, "Babar", 50.0, 0.001, "BALANCE_CHECK=10")};
    MemoryFragment mem("image", "memories/mountain.jpg", "Mountain trip", wallet.publicKey, 3600);
    chain.addBlock(txs, mem, wallet.publicKey, chain.getBalance(wallet.publicKey));
    chain.adjustDifficulty(txs[0].shardId);
}

int answer_to_connection(void* cls, struct MHD_Connection* connection, const char* url, 
                         const char* method, const char* version, const char* upload_data, 
                         size_t* upload_data_size, void** con_cls) {
    AhmiyatChain* chain = static_cast<AhmiyatChain*>(cls);
    std::string response;
    if (std::string(method) == "GET") {
        if (std::string(url) == "/balance") {
            const char* addr = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "address");
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response = std::to_string(chain->getBalance(addr ? addr : "genesis", shard ? shard : "0"));
        } else if (std::string(url) == "/shard") {
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response = chain->getShardStatus(shard ? shard : "0");
        }
    } else if (std::string(method) == "POST" && std::string(url) == "/tx") {
        if (*upload_data_size) {
            try {
                std::string data(upload_data, *upload_data_size);
                std::stringstream ss(data);
                std::string sender;
                double amount;
                std::getline(ss, sender, '&');
                ss >> amount;
                Transaction tx(sender, "receiver", amount, 0.001);
                chain->addPendingTx(tx);
                response = "Transaction queued";
            } catch (const std::exception& e) {
                response = "Invalid transaction: " + std::string(e.what());
            }
            *upload_data_size = 0;
        }
    }

    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(response.length(), 
                                                                       (void*)response.c_str(), 
                                                                       MHD_RESPMEM_MUST_COPY);
    int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
    MHD_destroy_response(mhd_response);
    return ret;
}

void runAPI(AhmiyatChain& chain) {
    struct MHD_Daemon* daemon = MHD_start_daemon(MHD_USE_EPOLL | MHD_USE_THREAD_PER_CONNECTION, 
                                                 8080, NULL, NULL, &answer_to_connection, 
                                                 &chain, MHD_OPTION_END);
    if (!daemon) {
        log("Failed to start API server");
        return;
    }
    log("API server running on port 8080");
    while (keepRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    MHD_stop_daemon(daemon);
}

void loadConfig(AhmiyatChain& chain, const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        log("Failed to open config file: " + configFile);
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("node:") == 0) {
            std::string id, ip;
            int port;
            std::stringstream ss(line.substr(5));
            std::getline(ss, id, ',');
            std::getline(ss, ip, ',');
            ss >> port;
            chain.addNode(id, ip, port);
        } else if (line.find("bootstrap:") == 0) {
            std::string ip;
            int port;
            std::stringstream ss(line.substr(10));
            std::getline(ss, ip, ',');
            ss >> port;
            chain.dht.bootstrap(ip, port);
        }
    }
    file.close();
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    if (argc < 2) {
        log("Usage: ./ahmiyat <port>");
        return 1;
    }
    int port = std::atoi(argv[1]);

    system("mkdir -p memories");

    AhmiyatChain ahmiyat;
    loadConfig(ahmiyat, "config.txt");

    std::thread nodeThread(runNode, std::ref(ahmiyat), port);
    std::thread minerThread(mineBlock, std::ref(ahmiyat), "Miner" + std::to_string(port));
    std::thread apiThread(runAPI, std::ref(ahmiyat));

    minerThread.join();
    ahmiyat.stressTest(10);

    log("Balance of genesis: " + std::to_string(ahmiyat.getBalance("genesis")));
    log("Optimized node running on port " + std::to_string(port));

    apiThread.join();
    nodeThread.detach();
    return 0;
}
