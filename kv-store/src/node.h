#pragma once
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <memory>
#include "cache.h"
#include "wal.h"
#include "protocol.h"

class KVNode {
public:
    KVNode(const std::string& node_id,
           const std::string& data_dir,
           size_t cache_size = 10000);

    ~KVNode() = default;

    // Core operations
    bool        put(const std::string& key,
                    const std::string& value,
                    int ttl = -1);

    std::optional<std::string> get(const std::string& key);

    bool        del(const std::string& key);

    // Stats
    struct Stats {
        uint64_t total_keys;
        uint64_t cache_hits;
        uint64_t cache_misses;
        uint64_t puts;
        uint64_t gets;
        uint64_t deletes;
        double   cache_hit_rate;
    };

    Stats get_stats() const;

    std::string node_id() const { return node_id_; }

    void recover();  // replay WAL on startup

private:
    std::string node_id_;
    std::string data_dir_;

    // Main storage: key → value
    std::unordered_map<std::string, std::string> store_;
    mutable std::shared_mutex                    store_mutex_;

    std::unique_ptr<LRUCache> cache_;
    std::unique_ptr<WAL>      wal_;

    std::atomic<uint64_t> puts_{0};
    std::atomic<uint64_t> gets_{0};
    std::atomic<uint64_t> deletes_{0};
};
