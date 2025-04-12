#include "blockchain.h"
#include <thread>
#include <iostream>
#include <microhttpd.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    json response = {};
    int status_code = MHD_HTTP_OK;
    if (std::string(method) == "GET") {
        if (std::string(url) == "/balance") {
            const char* addr = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "address");
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response["balance"] = chain->getBalance(addr ? addr : "genesis", shard ? shard : "0");
        } else if (std::string(url) == "/shard") {
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            response["status"] = chain->getShardStatus(shard ? shard : "0");
        } else if (std::string(url) == "/metrics") {
            std::stringstream metrics;
            metrics << "ahmiyat_blocks_total{shard=\"0\"} " << chain->getShardStatus("0").size() << "\n";
            std::string metrics_str = metrics.str();
            struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
                metrics_str.length(), (void*)metrics_str.c_str(), MHD_RESPMEM_MUST_COPY
            );
            MHD_add_response_header(mhd_response, "Content-Type", "text/plain");
            int ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
            MHD_destroy_response(mhd_response);
            return ret;
        } else {
            status_code = MHD_HTTP_NOT_FOUND;
            response["error"] = "Endpoint not found";
        }
    } else if (std::string(method) == "POST" && std::string(url) == "/tx") {
        if (*upload_data_size) {
            try {
                json txData = json::parse(std::string(upload_data, *upload_data_size));
                Transaction tx(
                    txData["sender"].get<std::string>(),
                    txData["receiver"].get<std::string>(),
                    txData["amount"].get<double>(),
                    txData.value("fee", 0.001),
                    txData.value("script", ""),
                    txData.value("shardId", "0")
                );
                chain->addPendingTx(tx);
                response["message"] = "Transaction queued";
                *upload_data_size = 0;
            } catch (const json::exception& e) {
                status_code = MHD_HTTP_BAD_REQUEST;
                response["error"] = "Invalid JSON: " + std::string(e.what());
            }
        }
    } else {
        status_code = MHD_HTTP_METHOD_NOT_ALLOWED;
        response["error"] = "Method not allowed";
    }
    std::string response_str = response.dump();
    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
        response_str.length(), (void*)response_str.c_str(), MHD_RESPMEM_MUST_COPY
    );
    MHD_add_response_header(mhd_response, "Content-Type", "application/json");
    MHD_add_response_header(mhd_response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(mhd_response, "Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    int ret = MHD_queue_response(connection, status_code, mhd_response);
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
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1000));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log("Usage: ./ahmiyat <port>");
        return 1;
    }
    int port = atoi(argv[1]);

    system("mkdir -p memories");

    AhmiyatChain ahmiyat;

    ahmiyat.addNode("Node3", "node3.ahmiyat.example.com", 5003);
    ahmiyat.dht.bootstrap("node1.ahmiyat.example.com", 5001);

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
