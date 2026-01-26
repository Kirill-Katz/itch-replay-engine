#pragma once

#include <iostream>

#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_ip4.h>
#include <rte_udp.h>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <emmintrin.h>
#include <x86intrin.h>
#include <rte_mbuf_core.h>
#include "itch_header_parser.hpp"
#include "mold_udp_64.hpp"

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

    void handle(std::byte const * msg_start, const ITCH::ItchHeader& header, uint16_t header_size);
    void generate_network_headers(size_t payload_len);
    void setup_headers();
    void add_ethernet_header(std::byte * dst);
    rte_ipv4_hdr* add_ipv4_headers(std::byte * dst, size_t payload_size);
    rte_udp_hdr* add_udp_headers(std::byte * dst, size_t payload_size);
    void queue_packet();
    void send_packets();

private:
    uint64_t last_timestamp_ = 0;
    MoldUDP64 mold_udp_64_;

    std::vector<std::byte> payload_;
    uint16_t payload_msg_count_ = 0;

    static constexpr size_t packet_queue_size = 64;
    std::array<rte_mbuf*, packet_queue_size> buffers_;
    uint64_t queued_packets_ = 0;

    rte_mempool* mempool_;
    uint16_t port_;
    uint64_t total_messages_len = 0;

    struct rte_ipv4_hdr ipv4_hdr_;
    struct rte_ether_hdr ethernet_hdr_;
    struct rte_udp_hdr udp_hdr_;

    uint64_t total1 = 0;
    uint64_t total2 = 0;
    uint64_t total3 = 0;

    uint64_t count1 = 0;
    uint64_t count2 = 0;
    uint64_t count3 = 0;
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
    ipv4_hdr_.total_length = 0;

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
    udp_hdr_.dgram_len = 0;
    udp_hdr_.dgram_cksum = 0; // checksum will be computed when payload_length and payload will be known
}

inline void Handler::add_ethernet_header(std::byte * dst) {
    std::memcpy(dst, &ethernet_hdr_, sizeof(rte_ether_hdr));
}

inline rte_ipv4_hdr* Handler::add_ipv4_headers(std::byte* dst, size_t payload_size) {
    struct rte_ipv4_hdr ipv4_hdr = ipv4_hdr_;
    ipv4_hdr.total_length = rte_cpu_to_be_16(
        sizeof(rte_ipv4_hdr) +
        sizeof(rte_udp_hdr) +
        payload_size
    );

    ipv4_hdr.hdr_checksum = rte_ipv4_cksum(&ipv4_hdr);
    std::memcpy(dst, &ipv4_hdr, sizeof(rte_ipv4_hdr));
    rte_ipv4_hdr* ipv4_hdr_ptr = reinterpret_cast<rte_ipv4_hdr*>(dst);

    return ipv4_hdr_ptr;
}

inline rte_udp_hdr* Handler::add_udp_headers(std::byte* dst, size_t payload_size) {
    struct rte_udp_hdr udp_hdr = udp_hdr_;
    udp_hdr.dgram_len = rte_cpu_to_be_16(
        sizeof(rte_udp_hdr) + payload_size
    );

    std::memcpy(dst, &udp_hdr, sizeof(rte_udp_hdr));
    rte_udp_hdr* udp_hdr_ptr = reinterpret_cast<rte_udp_hdr*>(dst);
    return udp_hdr_ptr;
}

inline void Handler::queue_packet() {
    auto mold_udp_header = mold_udp_64_.generate_header(payload_msg_count_);
    const size_t payload_size = payload_.size() + mold_udp_header.size();
    size_t pkt_len =
        sizeof(rte_ether_hdr) +
        sizeof(rte_ipv4_hdr) +
        sizeof(rte_udp_hdr) +
        payload_size;

    rte_mbuf* m = rte_pktmbuf_alloc(mempool_);
    if (!m) {
        throw std::runtime_error("Failed to allocate a pkt buffer!");
    }

    std::byte* p = reinterpret_cast<std::byte*>(rte_pktmbuf_append(m, pkt_len));
    if (!p) {
        rte_pktmbuf_free(m);
        throw std::runtime_error("Failed to append packet data to mbuf!");
    }

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

    buffers_[queued_packets_] = m;
    queued_packets_++;
}

inline void Handler::send_packets() {
    size_t base = 0;
    while (queued_packets_ > 0) {
        auto sent_packets = rte_eth_tx_burst(port_, 0, buffers_.data() + base, queued_packets_);
        if (sent_packets == 0) {
            continue;
        }

        queued_packets_ -= sent_packets;
        base += sent_packets;
    }
}

inline void Handler::handle(std::byte const * msg_start, const ITCH::ItchHeader& header, uint16_t message_size) {
    count1++;
    unsigned aux = 0;
    uint64_t start1 = __rdtscp(&aux);

    total_messages_len += message_size;

    bool is_last_message = header.type == 'S' && static_cast<char>(*(msg_start + 13)) == 'C';
    payload_.insert(payload_.end(), msg_start, msg_start + message_size);
    payload_msg_count_++;

    uint64_t end1 = __rdtscp(&aux);
    total1 += end1 - start1;

    bool should_pack = payload_.size() >= 1400 || is_last_message;
    if (should_pack) {
        count2++;
        uint64_t start2 = __rdtscp(&aux);
        queue_packet();
        payload_.clear();
        payload_msg_count_ = 0;
        uint64_t end2 = __rdtscp(&aux);
        total2 += end2 - start2;
    }

    bool should_send = queued_packets_ == packet_queue_size || is_last_message;
    if (should_send) {
        count3++;
        uint64_t start3 = __rdtscp(&aux);
        send_packets();
        uint64_t end3 = __rdtscp(&aux);
        total3 += end3 - start3;
    }

    if (is_last_message) {
        std::cout << total1 / count1 << '\n';
        std::cout << total2 / count2 << '\n';
        std::cout << total3 / count3 << '\n';
    }
}
