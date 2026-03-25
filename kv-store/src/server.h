#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>
#include "thread_pool.h"
#include "node.h"
#include "consistent_hash.h"
#include "protocol.h"

struct ServerConfig {
    std::string host      = "0.0.0.0";
    int         port      = 7777;
    int         threads   = 8;
    size_t      cache_mb  = 256;
    std::string data_dir  = "./kv_data";
    int         repl_factor = 2;
};

class KVServer {
public:
    explicit KVServer(const ServerConfig& cfg);
    ~KVServer();

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    ServerConfig              config_;
    std::unique_ptr<ThreadPool>   thread_pool_;
    std::unique_ptr<KVNode>       local_node_;
    std::atomic<bool>             running_{false};
    int                           server_fd_{-1};

    void accept_loop();
    void handle_client(int client_fd);
    Response dispatch(const Request& req);

    // Read exactly n bytes
    bool read_exact(int fd, uint8_t* buf, size_t n);
    bool send_all(int fd, const uint8_t* buf, size_t n);

    void setup_socket();
    void log(const std::string& msg);
};
