#pragma once

#include <cstdint>
#include <cstring>
#include <array>

class MoldUDP64 {
public:
    MoldUDP64(const char* session_name, uint64_t seq_start)
    : current_seq_(seq_start) {
        std::memcpy(session_name_, session_name, 10);
    }

    std::array<std::byte, 20> generate_header(uint16_t msg_count);

private:
    char session_name_[10];
    uint64_t current_seq_;
};
