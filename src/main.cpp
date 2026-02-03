#include <atomic>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <iostream>
#include <rte_lcore.h>
#include <rte_mbuf_core.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <x86intrin.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <fstream>
#include <rte_service.h>

#include "dpdk_context.hpp"
#include "spsc_buffer.hpp"
#include "handler.hpp"
#include "itch_header_parser.hpp"

std::atomic<bool> consume;
std::atomic<size_t> total_bytes = 0;
constexpr size_t _2MB = 2*1024*1024;

void consumer(SPSCBuffer& ring_buffer, rte_mempool* mempool, uint16_t port_id) {
    auto buffer = std::make_unique<std::byte[]>(_2MB);
    Handler handler(mempool, port_id);
    ITCH::ItchHeaderParser parser;

    std::ofstream out("../data/itch_out_producer",
                  std::ios::binary | std::ios::out | std::ios::trunc);
    std::vector<char> buf;
    buf.reserve(1<<20);

    size_t unparsed_bytes = 0;

    while (true) {
        std::span dst(buffer.get() + unparsed_bytes, _2MB - unparsed_bytes);
        size_t read = ring_buffer.read(dst);

        if (!read) {
            if (!consume.load(std::memory_order_acquire)) {
                break;
            }
            _mm_pause();
            continue;
        }

        out.write(reinterpret_cast<char*>(dst.data()), read);

        size_t total = unparsed_bytes + read;
        size_t parsed_bytes = parser.parse(buffer.get(), total, handler);
        unparsed_bytes = total - parsed_bytes;

        if (unparsed_bytes >= _2MB) {
            std::cerr << "Something went seriously wrong!" << '\n';
            std::abort();
        }

        std::memmove(buffer.get(), buffer.get() + parsed_bytes, unparsed_bytes);
        total_bytes.fetch_add(read, std::memory_order_relaxed);
    }

    out.flush();
}

int main(int argc, char** argv) {
    constexpr uint16_t port_id = 0;
    DPDKContext dpdk_context(port_id);

    dpdk_context.setup_eal(argc, argv);
    dpdk_context.setup_mempool();
    dpdk_context.setup_eth_device(0);

    std::string itch_file_path = argv[1];
    consume.store(true, std::memory_order_relaxed);

    int fd = open(itch_file_path.data(), O_RDONLY);
    struct stat st;
    if(fstat(fd, &st) < 0) {
        std::cerr << "Fstat failed" << '\n';
        std::abort();
    }

    size_t size = st.st_size;
    void* ptr = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Map failed" << '\n';
        std::abort();
    }

    madvise(ptr, size, MADV_WILLNEED | MADV_SEQUENTIAL);
    std::byte* src = static_cast<std::byte*>(ptr);

    auto start = std::chrono::steady_clock::now();
    Handler handler(dpdk_context.get_pool(), port_id);
    ITCH::ItchHeaderParser parser;

    parser.parse(src, size, handler);

    munmap(ptr, size);
    close(fd);

    auto end = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();
    size_t bytes = total_bytes.load();

    double gb = bytes / 1e9;
    double gbps = gb / seconds;

    std::cout << "Processed: " << bytes << " bytes\n";
    std::cout << "Time: " << seconds << " s\n";
    std::cout << "Throughput: " << gbps << " GB/s\n";

    rte_eth_dev_stop(port_id);
    rte_eth_dev_close(port_id);

    return 0;
}
