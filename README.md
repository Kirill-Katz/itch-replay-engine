# ITCH Market Data Replay Engine

High-throughput replay of ITCH feeds using DPDK vdev

# Build
```
mkdir build/
cd build
cmake ..
make
```

# Run
Notice that the consumer should setup up the DPDK vdev device and the producer will be a "secondary device".
Also for more info check the [dpdk quick start](https://core.dpdk.org/doc/quick-start/) to setup hugepages and install dpdk.

```
sudo ./run  --proc-type=primary --file-prefix=memif_srv  --vdev=net_memif0,socket=/tmp/memif.sock,id=0,role=server,rsize=14   --log-level=pmd.net.memif,8   [ITCH file path]
```

To see how to implement a consumer go to [this](https://github.com/Kirill-Katz/itch5-hft-parser/tree/dpdk-enabled-version) repo to the `dpdk-enabled-version` to see an example.
