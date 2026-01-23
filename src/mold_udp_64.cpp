#include <cstring>
#include <rte_byteorder.h>
#include "mold_udp_64.hpp"

std::array<std::byte, 20> MoldUDP64::generate_header(uint16_t msg_count) {
    std::array<std::byte, 20> header;
    std::memcpy(header.data(), session_name_, 10);

    auto seq_num = rte_cpu_to_be_64(current_seq_);
    std::memcpy(header.data()+10, &seq_num, 8);

    auto msg_count_be = rte_cpu_to_be_16(msg_count);
    std::memcpy(header.data()+18, &msg_count_be, 2);

    current_seq_ += 1;
    return header;
}

