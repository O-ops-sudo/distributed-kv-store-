#pragma once
#include <map>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <optional>

struct Node {
    std::string id;
    std::string host;
    int         port;
    bool        alive{true};

    std::string to_string() const {
        return id + "@" + host + ":" + std::to_string(port);
    }
};

class ConsistentHash {
public:
    // virtual_nodes = how many points per real node on the ring
    explicit ConsistentHash(int virtual_nodes = 150);

    void add_node(const Node& node);
    void remove_node(const std::string& node_id);

    // Get primary node for key
    std::optional<Node> get_node(const std::string& key) const;

    // Get N nodes for replication
    std::vector<Node> get_nodes(const std::string& key, int count) const;

    int node_count() const;

private:
    int                         virtual_nodes_;
    std::map<uint32_t, Node>    ring_;          // hash → node
    mutable std::mutex          mutex_;

    uint32_t hash(const std::string& key) const;
    std::string virtual_key(const std::string& node_id, int i) const;
};
