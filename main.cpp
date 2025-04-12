#include "blockchain.h"
#include <nlohmann/json.hpp>
#include <microhttpd.h>
#include <sstream>

using json = nlohmann::json;
extern void log(const std::string& message);

MHD_Result answer_to_connection(void* cls, struct MHD_Connection* connection, const char* url,
                                const char* method, const char* version, const char* upload_data,
                                size_t* upload_data_size, void** con_cls) {
    AhmiyatChain* chain = static_cast<AhmiyatChain*>(cls);
    std::string response;
    MHD_Result ret;

    if (std::string(method) == "GET") {
        if (std::string(url) == "/balance") {
            const char* address = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "address");
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            std::string shardId = shard ? shard : "0";
            double balance = chain->getBalance(address ? address : "", shardId);
            json j = {{"balance", balance}, {"shard", shardId}};
            response = j.dump();
        } else if (std::string(url) == "/status") {
            const char* shard = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "shard");
            std::string shardId = shard ? shard : "0";
            response = chain->getShardStatus(shardId);
        } else if (std::string(url) == "/metrics") {
            std::ostringstream metrics;
            metrics << "Total Shards: " << chain->getShardStatus("0").size() << "\n";
            response = metrics.str();
        } else {
            response = "Invalid endpoint";
        }

        struct MHD_Response* mhd_response = MHD_create_response_from_buffer(response.length(),
                                                                            (void*)response.c_str(),
                                                                            MHD_RESPMEM_MUST_COPY);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
        MHD_destroy_response(mhd_response);
        return ret;
    }

    if (std::string(method) == "POST" && std::string(url) == "/tx") {
        if (*upload_data_size != 0) {
            std::string data(upload_data, *upload_data_size);
            *upload_data_size = 0;
            try {
                json j = json::parse(data);
                Transaction tx(j["sender"], j["receiver"], j["amount"]);
                chain->addPendingTx(tx);
                response = "Transaction added";
            } catch (const std::exception& e) {
                response = "Invalid transaction data";
            }

            struct MHD_Response* mhd_response = MHD_create_response_from_buffer(response.length(),
                                                                                (void*)response.c_str(),
                                                                                MHD_RESPMEM_MUST_COPY);
            ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
            MHD_destroy_response(mhd_response);
            return ret;
        }
        return MHD_YES;
    }

    response = "Method not supported";
    struct MHD_Response* mhd_response = MHD_create_response_from_buffer(response.length(),
                                                                        (void*)response.c_str(),
                                                                        MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, mhd_response);
    MHD_destroy_response(mhd_response);
    return ret;
}

void runAPI(AhmiyatChain& chain) {
    struct MHD_Daemon* daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 8080, nullptr, nullptr,
                                                 &answer_to_connection, &chain, MHD_OPTION_END);
    if (!daemon) {
        log("Failed to start API server");
        return;
    }
    log("API server running on port 8080");
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    MHD_stop_daemon(daemon);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        log("Usage: ./ahmiyat <port>");
        return 1;
    }

    int sysResult = system("mkdir -p memories");
    if (sysResult != 0) {
        log("Failed to create memories directory");
    }
    int port = std::stoi(argv[1]);
    AhmiyatChain ahmiyat;
    ahmiyat.dht.bootstrap("node1.ahmiyat.example.com", 5001);

    std::thread apiThread(runAPI, std::ref(ahmiyat));
    apiThread.detach();

    log("Balance of genesis: " + std::to_string(ahmiyat.getBalance("genesis", "0")));
    log("Optimized node running on port " + std::to_string(port));
    ahmiyat.startNodeListener(port);

    return 0;
}
