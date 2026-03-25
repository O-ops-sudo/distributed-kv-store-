#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <thread>
#include <atomic>
#include "../src/protocol.h"

class KVClient {
public:
    KVClient(const std::string& host, int port) {
        fd_ = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (connect(fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("connect() failed");
        }

        // Set timeout
        struct timeval tv{5, 0};
        setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }

    ~KVClient() { if (fd_ >= 0) close(fd_); }

    bool put(const std::string& key, const std::string& value) {
        Request req;
        req.type   = MsgType::PUT;
        req.req_id = next_id_++;
        req.key    = key;
        req.value  = value;

        send_request(req);
        auto resp = recv_response();
        return resp.status == Status::OK;
    }

    std::string get(const std::string& key) {
        Request req;
        req.type   = MsgType::GET;
        req.req_id = next_id_++;
        req.key    = key;

        send_request(req);
        auto resp = recv_response();
        if (resp.status == Status::NOT_FOUND) return "(not found)";
        return resp.value;
    }

    std::string stats() {
        Request req;
        req.type   = MsgType::STATS;
        req.req_id = next_id_++;

        send_request(req);
        auto resp = recv_response();
        return resp.value;
    }

private:
    int      fd_{-1};
    uint64_t next_id_{1};

    void send_request(const Request& req) {
        auto bytes = req.serialize();
        uint32_t len = (uint32_t)bytes.size();

        uint8_t len_buf[4] = {
            (uint8_t)(len >> 24), (uint8_t)(len >> 16),
            (uint8_t)(len >> 8),  (uint8_t)(len)
        };

        send(fd_, len_buf, 4, MSG_NOSIGNAL);
        send(fd_, bytes.data(), bytes.size(), MSG_NOSIGNAL);
    }

    Response recv_response() {
        uint8_t len_buf[4];
        recv_exact(len_buf, 4);
        uint32_t len = ((uint32_t)len_buf[0] << 24) | ((uint32_t)len_buf[1] << 16)
                     | ((uint32_t)len_buf[2] << 8)  |  (uint32_t)len_buf[3];

        std::vector<uint8_t> buf(len);
        recv_exact(buf.data(), len);
        return Response::deserialize(buf.data(), len);
    }

    void recv_exact(uint8_t* buf, size_t n) {
        size_t total = 0;
        while (total < n) {
            ssize_t r = recv(fd_, buf + total, n - total, 0);
            if (r <= 0) throw std::runtime_error("recv failed");
            total += r;
        }
    }
};

// Benchmark: concurrent throughput test
void benchmark(const std::string& host, int port) {
    const int NUM_THREADS = 8;
    const int OPS_PER_THREAD = 10000;

    std::atomic<uint64_t> total_ops{0};
    std::atomic<uint64_t> errors{0};

    auto t0 = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            try {
                KVClient client(host, port);
                std::mt19937 rng(t * 12345);

                for (int i = 0; i < OPS_PER_THREAD; ++i) {
                    std::string key   = "key:" + std::to_string(rng() % 1000);
                    std::string value = "value:" + std::to_string(i);

                    if (i % 3 == 0) {
                        client.put(key, value);
                    } else {
                        client.get(key);
                    }
                    total_ops.fetch_add(1);
                }
            } catch (const std::exception& e) {
                std::cerr << "Thread error: " << e.what() << "\n";
                errors.fetch_add(1);
            }
        });
    }

    for (auto& th : threads) th.join();

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "\n── Benchmark Results ──────────────\n";
    std::cout << "Total ops:  " << total_ops.load() << "\n";
    std::cout << "Errors:     " << errors.load()    << "\n";
    std::cout << "Time:       " << elapsed << "s\n";
    std::cout << "Throughput: " << (uint64_t)(total_ops / elapsed) << " ops/sec\n";
    std::cout << "───────────────────────────────────\n";
}

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 7777;

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = std::stoi(argv[2]);

    if (argc >= 4 && std::string(argv[3]) == "--bench") {
        benchmark(host, port);
        return 0;
    }

    // Interactive mode
    KVClient client(host, port);
    std::cout << "KV Client — connected to " << host << ":" << port << "\n";
    std::cout << "Commands: GET <key>, PUT <key> <value>, STATS, EXIT\n\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line == "EXIT") break;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "GET") {
            std::string key;
            iss >> key;
            std::cout << client.get(key) << "\n";

        } else if (cmd == "PUT") {
            std::string key, val;
            iss >> key >> val;
            bool ok = client.put(key, val);
            std::cout << (ok ? "OK" : "ERROR") << "\n";

        } else if (cmd == "STATS") {
            std::cout << client.stats() << "\n";

        } else {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}
