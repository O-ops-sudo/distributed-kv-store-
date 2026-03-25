#include "consistent_hash.h"
#include <stdexcept>
#include <sstream>

// FNV-1a hash — fast, good distribution
static uint32_t fnv1a(const std::string& s) {
    uint32_t hash = 2166136261u;
    for (unsigned char c : s) {
        hash ^= c;
        hash *= 16777619u;
    }
    return hash;
}

ConsistentHash::ConsistentHash(int virtual_nodes)
    : virtual_nodes_(virtual_nodes) {}

std::string ConsistentHash::virtual_key(const std::string& id, int i) const {
    return id + "#vnode" + std::to_string(i);
}

uint32_t ConsistentHash::hash(const std::string& key) const {
    return fnv1a(key);
}

void ConsistentHash::add_node(const Node& node) {
    std::unique_lock<std::mutex> lock(mutex_);
    for (int i = 0; i < virtual_nodes_; ++i) {
        uint32_t h = hash(virtual_key(node.id, i));
        ring_[h]   = node;
    }
}

void ConsistentHash::remove_node(const std::string& node_id) {
    std::unique_lock<std::mutex> lock(mutex_);
    for (int i = 0; i < virtual_nodes_; ++i) {
        uint32_t h = hash(virtual_key(node_id, i));
        ring_.erase(h);
    }
}

std::optional<Node> ConsistentHash::get_node(const std::string& key) const {
    std::unique_lock<std::mutex> lock(mutex_);
    if (ring_.empty()) return std::nullopt;

    uint32_t h   = hash(key);
    auto     it  = ring_.lower_bound(h);

    // Wrap around
    if (it == ring_.end()) it = ring_.begin();

    return it->second;
}

std::vector<Node> ConsistentHash::get_nodes(const std::string& key, int count) const {
    std::unique_lock<std::mutex> lock(mutex_);
    if (ring_.empty()) return {};

    std::vector<Node> result;
    std::vector<std::string> seen_ids;

    uint32_t h  = hash(key);
    auto     it = ring_.lower_bound(h);

    // Walk the ring, collect unique nodes
    auto start = it;
    bool wrapped = false;

    while ((int)result.size() < count) {
        if (it == ring_.end()) {
            if (wrapped) break;
            it      = ring_.begin();
            wrapped = true;
        }
        if (it == start && !result.empty()) break;

        const std::string& nid = it->second.id;
        bool already_seen = false;
        for (auto& s : seen_ids) {
            if (s == nid) { already_seen = true; break; }
        }

        if (!already_seen && it->second.alive) {
            result.push_back(it->second);
            seen_ids.push_back(nid);
        }
        ++it;
    }

    return result;
}

int ConsistentHash::node_count() const {
    std::unique_lock<std::mutex> lock(mutex_);
    // Count unique nodes
    std::vector<std::string> seen;
    for (auto& [h, node] : ring_) {
        bool found = false;
        for (auto& s : seen) if (s == node.id) { found = true; break; }
        if (!found) seen.push_back(node.id);
    }
    return (int)seen.size();
}
