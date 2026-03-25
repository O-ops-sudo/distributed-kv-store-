#pragma once
#include <string>
#include <cstdint>
#include <vector>

// Wire protocol (binary, simple)
// Request:  [msg_type:1][req_id:8][key_len:2][key][val_len:4][val]
// Response: [status:1][req_id:8][val_len:4][val]

enum class MsgType : uint8_t {
    GET    = 1,
    PUT    = 2,
    DELETE = 3,
    PING   = 4,
    STATS  = 5,
};

enum class Status : uint8_t {
    OK        = 0,
    NOT_FOUND = 1,
    ERROR     = 2,
    PONG      = 3,
};

struct Request {
    MsgType     type;
    uint64_t    req_id;
    std::string key;
    std::string value;
    int         ttl{-1};

    std::vector<uint8_t> serialize() const;
    static Request deserialize(const uint8_t* buf, size_t len);
};

struct Response {
    Status      status;
    uint64_t    req_id;
    std::string value;

    std::vector<uint8_t> serialize() const;
    static Response deserialize(const uint8_t* buf, size_t len);

    static Response ok(uint64_t id, const std::string& val = "");
    static Response not_found(uint64_t id);
    static Response error(uint64_t id, const std::string& msg = "");
};
