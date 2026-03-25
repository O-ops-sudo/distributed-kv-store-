#include "wal.h"
#include <cstring>
#include <stdexcept>
#include <iostream>
#include <fstream>

// Simple CRC32-like checksum (not full CRC32, good enough)
uint32_t WAL::checksum(const WALEntry& e) {
    uint32_t h = 2166136261u;
    auto mix = [&](const std::string& s) {
        for (unsigned char c : s) {
            h ^= c;
            h *= 16777619u;
        }
    };
    mix(e.key);
    mix(e.value);
    h ^= (uint32_t)e.op;
    h *= 16777619u;
    return h;
}

WAL::WAL(const std::string& filepath) : filepath_(filepath) {
    file_.open(filepath_, std::ios::binary | std::ios::app);
    if (!file_.is_open()) {
        throw std::runtime_error("WAL: cannot open file: " + filepath_);
    }
}

WAL::~WAL() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

bool WAL::write_entry(WALOpType op,
                      const std::string& key,
                      const std::string& value)
{
    std::unique_lock<std::mutex> lock(mutex_);

    WALEntry entry;
    entry.sequence = ++sequence_;
    entry.op       = op;
    entry.key      = key;
    entry.value    = value;

    uint32_t csum = checksum(entry);

    // Write: seq | op | key_len | key | val_len | val | checksum
    file_.write(reinterpret_cast<const char*>(&entry.sequence), 8);
    file_.write(reinterpret_cast<const char*>(&op), 1);

    uint32_t klen = (uint32_t)key.size();
    file_.write(reinterpret_cast<const char*>(&klen), 4);
    file_.write(key.data(), klen);

    uint32_t vlen = (uint32_t)value.size();
    file_.write(reinterpret_cast<const char*>(&vlen), 4);
    file_.write(value.data(), vlen);

    file_.write(reinterpret_cast<const char*>(&csum), 4);
    file_.flush();  // fsync for durability

    return file_.good();
}

bool WAL::append_put(const std::string& key, const std::string& value) {
    return write_entry(WALOpType::PUT, key, value);
}

bool WAL::append_delete(const std::string& key) {
    return write_entry(WALOpType::DELETE, key, "");
}

bool WAL::replay(std::function<void(const WALEntry&)> callback) {
    std::ifstream in(filepath_, std::ios::binary);
    if (!in.is_open()) return true;  // no log yet = ok

    uint64_t max_seq = 0;
    int entries_replayed = 0;

    while (in.peek() != EOF) {
        WALEntry entry;

        in.read(reinterpret_cast<char*>(&entry.sequence), 8);
        if (in.gcount() < 8) break;

        uint8_t op_raw;
        in.read(reinterpret_cast<char*>(&op_raw), 1);
        entry.op = static_cast<WALOpType>(op_raw);

        uint32_t klen;
        in.read(reinterpret_cast<char*>(&klen), 4);
        entry.key.resize(klen);
        in.read(entry.key.data(), klen);

        uint32_t vlen;
        in.read(reinterpret_cast<char*>(&vlen), 4);
        entry.value.resize(vlen);
        in.read(entry.value.data(), vlen);

        uint32_t stored_csum;
        in.read(reinterpret_cast<char*>(&stored_csum), 4);

        // Verify checksum
        if (checksum(entry) != stored_csum) {
            std::cerr << "[WAL] Checksum mismatch at seq="
                      << entry.sequence << ", stopping replay\n";
            break;
        }

        callback(entry);
        max_seq = std::max(max_seq, entry.sequence);
        ++entries_replayed;
    }

    sequence_ = max_seq;
    std::cout << "[WAL] Replayed " << entries_replayed << " entries\n";
    return true;
}

void WAL::sync() {
    std::unique_lock<std::mutex> lock(mutex_);
    file_.flush();
}

void WAL::rotate() {
    std::unique_lock<std::mutex> lock(mutex_);
    file_.close();

    std::string archive = filepath_ + "." + std::to_string(sequence_);
    std::rename(filepath_.c_str(), archive.c_str());

    file_.open(filepath_, std::ios::binary | std::ios::app);
}
