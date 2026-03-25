#include "server.h"
#include <iostream>
#include <csignal>
#include <thread>

static KVServer* g_server = nullptr;

void signal_handler(int sig) {
    std::cout << "\n[Main] Signal " << sig << " received, shutting down...\n";
    if (g_server) g_server->stop();
}

int main(int argc, char* argv[]) {
    ServerConfig cfg;

    // Parse simple CLI args
    for (int i = 1; i < argc; i += 2) {
        std::string flag = argv[i];
        if (i + 1 >= argc) break;
        std::string val = argv[i+1];

        if (flag == "--port")     cfg.port     = std::stoi(val);
        if (flag == "--threads")  cfg.threads  = std::stoi(val);
        if (flag == "--cache-mb") cfg.cache_mb = std::stoul(val);
        if (flag == "--data-dir") cfg.data_dir = val;
    }

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "═══════════════════════════════════\n";
    std::cout << "   Distributed KV Store v1.0\n";
    std::cout << "═══════════════════════════════════\n";

    try {
        KVServer server(cfg);
        g_server = &server;
        server.start();
    } catch (const std::exception& e) {
        std::cerr << "[Main] Fatal: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
