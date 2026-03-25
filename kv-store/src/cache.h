#pragma once
#include <unordered_map>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <chrono>

struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expires_at;
    bool has_ttl{false};
};

class LRUCache {
public:
    explicit LRUCache(size_t capacity);

    void put(const std::string& key,
             const std::string& value,
             int ttl_seconds = -1);

    std::optional<std::string> get(const std::string& key);
    bool remove(const std::string& key);
    void clear();

    size_t size() const;
    size_t capacity() const { return capacity_; }

    // Stats
    uint64_t hits()   const { return hits_;   }
    uint64_t misses() const { return misses_; }

private:
    size_t   capacity_;
    uint64_t hits_{0};
    uint64_t misses_{0};

    // LRU list: front = most recent
    using KVPair = std::pair<std::string, CacheEntry>;
    std::list<KVPair>                                      lru_list_;
    std::unordered_map<std::string, std::list<KVPair>::iterator> map_;
    mutable std::mutex                                     mutex_;

    void evict_lru();
    bool is_expired(const CacheEntry& e) const;
};
