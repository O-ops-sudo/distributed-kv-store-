#include "cache.h"

LRUCache::LRUCache(size_t capacity) : capacity_(capacity) {}

bool LRUCache::is_expired(const CacheEntry& e) const {
    if (!e.has_ttl) return false;
    return std::chrono::steady_clock::now() > e.expires_at;
}

void LRUCache::put(const std::string& key,
                   const std::string& value,
                   int ttl_seconds)
{
    std::unique_lock<std::mutex> lock(mutex_);

    auto it = map_.find(key);
    if (it != map_.end()) {
        // Update existing — move to front
        lru_list_.erase(it->second);
        map_.erase(it);
    }

    CacheEntry entry;
    entry.value   = value;
    entry.has_ttl = (ttl_seconds > 0);
    if (entry.has_ttl) {
        entry.expires_at = std::chrono::steady_clock::now()
                         + std::chrono::seconds(ttl_seconds);
    }

    lru_list_.push_front({key, entry});
    map_[key] = lru_list_.begin();

    if (map_.size() > capacity_) {
        evict_lru();
    }
}

std::optional<std::string> LRUCache::get(const std::string& key) {
    std::unique_lock<std::mutex> lock(mutex_);

    auto it = map_.find(key);
    if (it == map_.end()) {
        ++misses_;
        return std::nullopt;
    }

    auto& entry = it->second->second;

    if (is_expired(entry)) {
        lru_list_.erase(it->second);
        map_.erase(it);
        ++misses_;
        return std::nullopt;
    }

    // Move to front (most recently used)
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
    ++hits_;
    return entry.value;
}

bool LRUCache::remove(const std::string& key) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto it = map_.find(key);
    if (it == map_.end()) return false;
    lru_list_.erase(it->second);
    map_.erase(it);
    return true;
}

void LRUCache::clear() {
    std::unique_lock<std::mutex> lock(mutex_);
    lru_list_.clear();
    map_.clear();
}

size_t LRUCache::size() const {
    std::unique_lock<std::mutex> lock(mutex_);
    return map_.size();
}

void LRUCache::evict_lru() {
    // Called with lock held
    auto last = lru_list_.end();
    --last;
    map_.erase(last->first);
    lru_list_.pop_back();
}
