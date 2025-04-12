#include "blockchain.h"
#include <thread>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <microhttpd.h>

using namespace std; // std namespace use karenge

// Log function define karo
void log(const string& message) {
    cout << message << endl;
}

void runNode(AhmiyatChain& chain, int port) {
    chain.startNodeListener(port);
}

void mineBlock(AhmiyatChain& chain, string minerId) {
    Wallet wallet;
    vector<Transaction> txs = {Transaction(wallet.publicKey, "Babar", 50.0, 0.001, "BALANCE_CHECK=10")};
    MemoryFragment mem("image", "memories/mountain.jpg", "Mountain trip", wallet.publicKey, 3600);
    chain.addBlock(txs, mem, wallet.publicKey, chain.getBalance(wallet.publicKey));
    chain.adjustDifficulty(txs[0].shardId);
}

int answer_to_connection(void* cls, struct MHD_Connection* connection, const char* url, 
                         const char* method, const char* version, const char* upload_data, 
                         size_t* upload_data_size, void** con_cls) {
    AhmiyatChain* chain = static_cast<AhmiyatChain*>(cls);
    string response;
    if (string(method) == "GET") {
        if (string(url) == "/balance") {
            const char* addr = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "address");
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response = to_string(chain->getBalance(addr ? addr : "genesis", shard ? shard : "0"));
        } else if (string(url) == "/shard") {
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response = chain->getShardStatus(shard ? shard : "0");
        }
    } else if (string(method) == "POST" && string(url) == "/tx") {
        if (*upload_data_size) {
            Transaction tx(string(upload_data), "receiver", 0.0, 0.0, ""); // Simplified parsing
            chain->addPendingTx(tx);
            *upload_data_size = 0;
            response = "Transaction queued";
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
    while (true) this_thread::sleep_for(chrono::seconds(1000));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log("Usage: ./ahmiyat <port>");
        return 1;
    }
    int port = atoi(argv[1]);

    system("mkdir -p memories");

    AhmiyatChain ahmiyat;
    ahmiyat.initialize("genesis"); // Initialize chain with genesis block

    ahmiyat.addNode("Node1", "node1.ahmiyat.com", 5001);
    ahmiyat.addNode("Node2", "node2.ahmiyat.com", 5002);
    ahmiyat.dht.bootstrap("node1.ahmiyat.com", 5001);

    thread nodeThread(runNode, ref(ahmiyat), port);
    thread minerThread(mineBlock, ref(ahmiyat), "Miner" + to_string(port));
    thread apiThread(runAPI, ref(ahmiyat));

    minerThread.join();
    ahmiyat.stressTest(10);

    log("Balance of genesis: " + to_string(ahmiyat.getBalance("genesis")));
    log("Optimized node running on port " + to_string(port));

    apiThread.join();
    nodeThread.detach();
    return 0;
}
