#include "node.h"
#include <iostream>
#include <filesystem>

KVNode::KVNode(const std::string& node_id,
               const std::string& data_dir,
               size_t cache_size)
    : node_id_(node_id)
    , data_dir_(data_dir)
{
    std::filesystem::create_directories(data_dir_);

    std::string wal_path = data_dir_ + "/" + node_id_ + ".wal";
    wal_   = std::make_unique<WAL>(wal_path);
    cache_ = std::make_unique<LRUCache>(cache_size);

    recover();
}

void KVNode::recover() {
    wal_->replay([this](const WALEntry& entry) {
        if (entry.op == WALOpType::PUT) {
            std::unique_lock<std::shared_mutex> lock(store_mutex_);
            store_[entry.key] = entry.value;
        } else if (entry.op == WALOpType::DELETE) {
            std::unique_lock<std::shared_mutex> lock(store_mutex_);
            store_.erase(entry.key);
        }
    });
    std::cout << "[Node:" << node_id_ << "] Recovery complete. "
              << "Keys loaded: " << store_.size() << "\n";
}

bool KVNode::put(const std::string& key,
                 const std::string& value,
                 int ttl)
{
    // Write to WAL first (durability)
    if (!wal_->append_put(key, value)) {
        std::cerr << "[Node:" << node_id_ << "] WAL write failed\n";
        return false;
    }

    // Write to main store
    {
        std::unique_lock<std::shared_mutex> lock(store_mutex_);
        store_[key] = value;
    }

    // Update cache
    cache_->put(key, value, ttl);

    puts_.fetch_add(1);
    return true;
}

std::optional<std::string> KVNode::get(const std::string& key) {
    gets_.fetch_add(1);

    // L1: Check cache first
    auto cached = cache_->get(key);
    if (cached) return cached;

    // L2: Check main store
    std::shared_lock<std::shared_mutex> lock(store_mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;

    // Warm the cache
    cache_->put(key, it->second);
    return it->second;
}

bool KVNode::del(const std::string& key) {
    if (!wal_->append_delete(key)) return false;

    {
        std::unique_lock<std::shared_mutex> lock(store_mutex_);
        if (store_.erase(key) == 0) return false;
    }

    cache_->remove(key);
    deletes_.fetch_add(1);
    return true;
}

KVNode::Stats KVNode::get_stats() const {
    Stats s;
    {
        std::shared_lock<std::shared_mutex> lock(store_mutex_);
        s.total_keys = store_.size();
    }
    s.cache_hits   = cache_->hits();
    s.cache_misses = cache_->misses();
    s.puts         = puts_.load();
    s.gets         = gets_.load();
    s.deletes      = deletes_.load();

    uint64_t total = s.cache_hits + s.cache_misses;
    s.cache_hit_rate = total > 0
                     ? (double)s.cache_hits / total * 100.0
                     : 0.0;
    return s;
}
