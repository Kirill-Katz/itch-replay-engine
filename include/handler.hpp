#pragma once

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip4.h>
#include <rte_udp.h>
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

    inline void handle(std::byte const * msg_start, const ITCH::ItchHeader& header, uint16_t header_size);
    inline void generate_network_headers(size_t payload_len);
    inline void setup_headers();
    inline void add_ethernet_header(std::byte * dst);
    inline rte_ipv4_hdr* add_ipv4_headers(std::byte * dst, size_t payload_size);
    inline rte_udp_hdr* add_udp_headers(std::byte * dst, size_t payload_size);

private:
    uint64_t last_timestamp_ = 0;
    MoldUDP64 mold_udp_64_;

    std::vector<std::byte> payload_;
    uint16_t payload_msg_count_ = 0;
    rte_mempool* mempool_;
    uint16_t port_;

    struct rte_ipv4_hdr ipv4_hdr_;
    struct rte_ether_hdr ethernet_hdr_;
    struct rte_udp_hdr udp_hdr_;
};

inline void Handler::setup_headers() {
    // Ethernet header
    struct rte_ether_addr src_mac;
    rte_eth_macaddr_get(port_, &src_mac);

    ethernet_hdr_.src_addr = src_mac;
    ethernet_hdr_.dst_addr = {
        .addr_bytes = {0xAA,0xBB,0xCC,0xDD,0xEE,0xFF}
    };

    ethernet_hdr_.ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
    printf("Src MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", src_mac.addr_bytes[0], src_mac.addr_bytes[1], src_mac.addr_bytes[2], src_mac.addr_bytes[3], src_mac.addr_bytes[4], src_mac.addr_bytes[5]);

    // Ipv4 header
    ipv4_hdr_.version_ihl = (4 << 4) | 5;
    ipv4_hdr_.type_of_service = 0;
    // not changed to be because the payload size will be added as well and then converted to be
    ipv4_hdr_.total_length = sizeof(rte_ipv4_hdr) + sizeof(rte_udp_hdr);

    ipv4_hdr_.packet_id = 0;
    ipv4_hdr_.fragment_offset = 0;
    ipv4_hdr_.time_to_live = 64;
    ipv4_hdr_.next_proto_id = IPPROTO_UDP;

    ipv4_hdr_.dst_addr = rte_cpu_to_be_32(0x0A000001);
    ipv4_hdr_.src_addr = rte_cpu_to_be_32(0x0A000002);

    ipv4_hdr_.hdr_checksum = 0; // checksum will be computed when the total_length will be set properly

    // UDP header
    udp_hdr_.src_port = rte_cpu_to_be_16(7788);
    udp_hdr_.dst_port = rte_cpu_to_be_16(7788);

    // will be converted to be when the payload_size is added
    udp_hdr_.dgram_len = sizeof(rte_udp_hdr);
    udp_hdr_.dgram_cksum = 0; // checksum will be computed when payload_length and payload will be known
}

inline void Handler::add_ethernet_header(std::byte * dst) {
    std::memcpy(dst, &ethernet_hdr_, sizeof(rte_ether_hdr));
}

inline rte_ipv4_hdr* Handler::add_ipv4_headers(std::byte* dst, size_t payload_size) {
    struct rte_ipv4_hdr ipv4_hdr = ipv4_hdr_;
    ipv4_hdr.total_length = rte_cpu_to_be_16(ipv4_hdr.total_length + payload_size);
    ipv4_hdr.hdr_checksum = rte_ipv4_cksum(&ipv4_hdr);
    std::memcpy(dst, &ipv4_hdr, sizeof(rte_ipv4_hdr));
    rte_ipv4_hdr* ipv4_hdr_ptr = reinterpret_cast<rte_ipv4_hdr*>(dst);

    return ipv4_hdr_ptr;
}

inline rte_udp_hdr* Handler::add_udp_headers(std::byte* dst, size_t payload_size) {
    struct rte_udp_hdr udp_hdr = udp_hdr_;
    udp_hdr.dgram_len = rte_cpu_to_be_16(udp_hdr_.dgram_len + payload_size);
    std::memcpy(dst, &udp_hdr, sizeof(rte_udp_hdr));
    rte_udp_hdr* udp_hdr_ptr = reinterpret_cast<rte_udp_hdr*>(dst);
    return udp_hdr_ptr;
}

inline void Handler::handle(std::byte const * msg_start, const ITCH::ItchHeader& header, uint16_t header_size) {
    // sending a MoldUDP64 with msg_count set to 0 as the first message is intended, as such a message
    // is treated as a hearthbeat.
    if (header.timestamp == last_timestamp_) {
        payload_.insert(payload_.end(), msg_start, msg_start + header_size);
        payload_msg_count_++;
    } else {
        auto mold_udp_header = mold_udp_64_.generate_header(payload_msg_count_);
        const size_t payload_size = payload_.size() + mold_udp_header.size();
        rte_mbuf* m = rte_pktmbuf_alloc(mempool_);
        std::byte* p = rte_pktmbuf_mtod(m, std::byte*);

        add_ethernet_header(p);
        p += sizeof(rte_ether_hdr);

        rte_ipv4_hdr* ipv4_hdr_ptr = add_ipv4_headers(p, payload_size);
        p += sizeof(rte_ipv4_hdr);

        rte_udp_hdr* udp_hdr_ptr = add_udp_headers(p, payload_size);
        p += sizeof(rte_udp_hdr);

        std::memcpy(p, mold_udp_header.data(), mold_udp_header.size());
        p += mold_udp_header.size();

        std::memcpy(p, payload_.data(), payload_.size());
        udp_hdr_ptr->dgram_cksum = 0;
        udp_hdr_ptr->dgram_cksum = rte_ipv4_udptcp_cksum(ipv4_hdr_ptr, udp_hdr_ptr);

        // send message

        payload_.clear();
        payload_.insert(payload_.end(), msg_start, msg_start + header_size);
        payload_msg_count_ = 1;
    }
}
