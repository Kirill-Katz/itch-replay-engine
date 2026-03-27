# ITCH Market Data Replay Engine

High-throughput replay of ITCH feeds using DPDK memif.

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
sudo taskset -c 3 ./run  --proc-type=primary --file-prefix=memif_srv  --vdev=net_memif0,socket=/tmp/memif2.sock,id=0,role=client,rsize=9,zero-copy=yes --single-file-segments -l 3 --no-pci [ITCH file path]
```

And example of an ingestion engine to which the replay engine sends data to can be found [here](https://github.com/Kirill-Katz/itch-ingestion-engine).
