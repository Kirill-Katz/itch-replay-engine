#pragma once

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <vector>
#include <cstdint>
#include <emmintrin.h>
#include <x86intrin.h>
#include "itch_header_parser.hpp"
#include "mold_udp_64.hpp"
#include <rte_mbuf_core.h>

class Handler {
public:
    Handler(rte_mempool* mempool, uint16_t port):
        mold_udp_64_("ITCHFEED00", 0),
        mempool_(mempool),
        port_(port)
    {
        payload_.reserve(2*1024);
        setup_headers();
    }

    inline void handle(std::byte const * msg_start, ITCH::ItchHeader header, uint16_t header_size);
    inline void setup_headers();

private:
    uint64_t last_timestamp_ = 0;
    MoldUDP64 mold_udp_64_;

    std::vector<std::byte> payload_;
    uint16_t payload_msg_count_ = 0;
    rte_mempool* mempool_;
    uint16_t port_;

};

inline void Handler::setup_headers() {
    // Ethernet header
    struct rte_ether_addr src_mac;
    rte_eth_macaddr_get(port_, &src_mac);

    struct rte_ether_hdr ethernet_hdr;
    ethernet_hdr.src_addr = src_mac;
    ethernet_hdr.dst_addr = {
        .addr_bytes = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}
    };

    ethernet_hdr.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    printf("Src MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", src_mac.addr_bytes[0], src_mac.addr_bytes[1], src_mac.addr_bytes[2], src_mac.addr_bytes[3], src_mac.addr_bytes[4], src_mac.addr_bytes[5]);

    // Ipv4 header
    struct rte_ipv4_hdr ipv4_hdr;


}

inline void Handler::handle(std::byte const * msg_start, ITCH::ItchHeader header, uint16_t header_size) {
    if (header.timestamp == last_timestamp_) {
        payload_.insert(payload_.end(), msg_start, msg_start + header_size);
        payload_msg_count_++;
    } else {
        auto mold_udp_header = mold_udp_64_.generate_header(payload_msg_count_);


        // send payload_


        payload_.clear();

        payload_.insert(payload_.end(), msg_start, msg_start + header_size);
        payload_msg_count_ = 1;
    }

}
