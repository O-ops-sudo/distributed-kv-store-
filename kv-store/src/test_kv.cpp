#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include "cache.h"
#include "consistent_hash.h"
#include "node.h"

void test_lru_cache() {
    std::cout << "Testing LRU Cache...\n";
    LRUCache cache(3);

    cache.put("a", "1");
    cache.put("b", "2");
    cache.put("c", "3");

    assert(cache.get("a").has_value());
    assert(*cache.get("a") == "1");

    // Adding 4th should evict LRU (b, since a was just accessed)
    cache.put("d", "4");
    assert(!cache.get("b").has_value());  // b evicted
    assert(cache.get("d").has_value());

    std::cout << "  LRU eviction: PASS\n";

    // Test TTL
    cache.put("ttl_key", "val", 1);
    assert(cache.get("ttl_key").has_value());
    std::this_thread::sleep_for(std::chrono::seconds(2));
    assert(!cache.get("ttl_key").has_value());  // expired
    std::cout << "  TTL expiry: PASS\n";
}

void test_consistent_hash() {
    std::cout << "Testing Consistent Hash...\n";
    ConsistentHash ring(100);

    ring.add_node({"n1", "127.0.0.1", 7771});
    ring.add_node({"n2", "127.0.0.1", 7772});
    ring.add_node({"n3", "127.0.0.1", 7773});

    assert(ring.node_count() == 3);

    auto node = ring.get_node("some_key");
    assert(node.has_value());
    std::cout << "  Key 'some_key' → " << node->id << "\n";

    // Same key should always map to same node
    auto n1 = ring.get_node("consistent_key");
    auto n2 = ring.get_node("consistent_key");
    assert(n1->id == n2->id);
    std::cout << "  Deterministic routing: PASS\n";

    // Replication
    auto nodes = ring.get_nodes("replicated_key", 2);
    assert(nodes.size() == 2);
    assert(nodes[0].id != nodes[1].id);
    std::cout << "  Replication (2 nodes): PASS\n";

    // Remove node
    ring.remove_node("n2");
    assert(ring.node_count() == 2);
    std::cout << "  Node removal: PASS\n";
}

void test_kv_node() {
    std::cout << "Testing KV Node...\n";

    KVNode node("test-node", "/tmp/kv_test", 1000);

    assert(node.put("hello", "world"));
    auto val = node.get("hello");
    assert(val.has_value());
    assert(*val == "world");
    std::cout << "  PUT/GET: PASS\n";

    assert(node.del("hello"));
    assert(!node.get("hello").has_value());
    std::cout << "  DELETE: PASS\n";

    // Concurrent access test
    std::vector<std::thread> threads;
    std::atomic<int> errors{0};

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&node, i, &errors]() {
            for (int j = 0; j < 100; ++j) {
                std::string key = "key:" + std::to_string(i) + ":" + std::to_string(j);
                if (!node.put(key, "value")) errors++;
                if (!node.get(key)) errors++;
            }
        });
    }

    for (auto& t : threads) t.join();
    assert(errors.load() == 0);
    std::cout << "  Concurrent access (10 threads × 100 ops): PASS\n";
}

int main() {
    std::cout << "═══════════════════════════════\n";
    std::cout << "   KV Store Unit Tests\n";
    std::cout << "═══════════════════════════════\n\n";

    test_lru_cache();
    test_consistent_hash();
    test_kv_node();

    std::cout << "\n✓ All tests passed!\n";
    return 0;
}
