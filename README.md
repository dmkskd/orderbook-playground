# Work in progress
Playground for various orderbook data structures.

Aiming at implementing a minimal Binance C connectivity with the least possible dependencies.

Goals:
- keep it minimal
- build a simple orderbook
  - [x] populate with binance depth snapshots
  - [] implement orderbook scan using both C and assembly (ARM Neo)
  - [] use SIMD (Neo) where possible
- use LLMs to play with different OrderBook datastructures (Claude Sonnet 4)
  - build and run on various architectures
    - local macbook pro with limactl
    - AWS x86_64 and Graviton
      - ideally bare metal for better perf counters
- when connecting to Binance:
  - streaming using websockets
