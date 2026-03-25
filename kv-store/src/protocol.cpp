#include "protocol.h"
#include <cstring>
#include <stdexcept>

std::vector<uint8_t> Request::serialize() const {
    std::vector<uint8_t> buf;

    buf.push_back((uint8_t)type);

    // req_id
    for (int i = 7; i >= 0; --i)
        buf.push_back((req_id >> (i*8)) & 0xFF);

    // key
    uint16_t klen = (uint16_t)key.size();
    buf.push_back(klen >> 8);
    buf.push_back(klen & 0xFF);
    for (char c : key) buf.push_back((uint8_t)c);

    // value
    uint32_t vlen = (uint32_t)value.size();
    for (int i = 3; i >= 0; --i)
        buf.push_back((vlen >> (i*8)) & 0xFF);
    for (char c : value) buf.push_back((uint8_t)c);

    // ttl
    uint32_t t = (uint32_t)(ttl < 0 ? 0xFFFFFFFF : ttl);
    for (int i = 3; i >= 0; --i)
        buf.push_back((t >> (i*8)) & 0xFF);

    return buf;
}

Request Request::deserialize(const uint8_t* buf, size_t len) {
    if (len < 16) throw std::runtime_error("Request too short");
    Request r;
    size_t pos = 0;

    r.type = (MsgType)buf[pos++];

    r.req_id = 0;
    for (int i = 0; i < 8; ++i)
        r.req_id = (r.req_id << 8) | buf[pos++];

    uint16_t klen = ((uint16_t)buf[pos] << 8) | buf[pos+1];
    pos += 2;
    r.key.assign((const char*)buf + pos, klen);
    pos += klen;

    uint32_t vlen = 0;
    for (int i = 0; i < 4; ++i) vlen = (vlen << 8) | buf[pos++];
    r.value.assign((const char*)buf + pos, vlen);
    pos += vlen;

    uint32_t t = 0;
    for (int i = 0; i < 4; ++i) t = (t << 8) | buf[pos++];
    r.ttl = (t == 0xFFFFFFFF) ? -1 : (int)t;

    return r;
}

std::vector<uint8_t> Response::serialize() const {
    std::vector<uint8_t> buf;

    buf.push_back((uint8_t)status);

    for (int i = 7; i >= 0; --i)
        buf.push_back((req_id >> (i*8)) & 0xFF);

    uint32_t vlen = (uint32_t)value.size();
    for (int i = 3; i >= 0; --i)
        buf.push_back((vlen >> (i*8)) & 0xFF);
    for (char c : value) buf.push_back((uint8_t)c);

    return buf;
}

Response Response::deserialize(const uint8_t* buf, size_t len) {
    if (len < 13) throw std::runtime_error("Response too short");
    Response r;
    size_t pos = 0;

    r.status = (Status)buf[pos++];

    r.req_id = 0;
    for (int i = 0; i < 8; ++i)
        r.req_id = (r.req_id << 8) | buf[pos++];

    uint32_t vlen = 0;
    for (int i = 0; i < 4; ++i) vlen = (vlen << 8) | buf[pos++];
    r.value.assign((const char*)buf + pos, vlen);

    return r;
}

Response Response::ok(uint64_t id, const std::string& val) {
    return {Status::OK, id, val};
}

Response Response::not_found(uint64_t id) {
    return {Status::NOT_FOUND, id, ""};
}

Response Response::error(uint64_t id, const std::string& msg) {
    return {Status::ERROR, id, msg};
}
