# Matching Engine

A C++20 matching-engine project built incrementally to study order-book design, deterministic limit-order matching, and low-latency systems concepts.

The project currently focuses on correctness and clear system design first. Later stages will introduce deterministic matching, performance analysis, and event-driven architecture.

## Current Features

Stage 1 implements a correctness-first in-memory limit order book with:

- Integer-tick price representation
- Separate bid and ask price levels
- Price-priority ordering
- FIFO ordering within each price level
- Order insertion and cancellation
- Best bid and best ask queries
- Order-ID indexing for efficient cancellation
- Basic correctness tests for order-book invariants

Matching is intentionally deferred to Stage 2. 

## Project Structure

```text
matching-engine/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── include/
│   └── order_book.hpp
├── tests/
│   └── order_book_test.cpp
└── docs/
    └── order_book_design.md
```

## Requirements

- C++20-compatible compiler
- CMake 3.16 or newer

The project has been tested on Linux with GCC 11.4.0 and CMake 3.22.1

## Build

The project uses an out-of-source CMake build workflow so generated build files remain separate from the source tree.

### Debug

Configure and build:
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

Run the test executable directly:
```bash
./build-debug/order_book_test
```

### Release 

Configure and build:
```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

The Release configuration will be used for future performance and benchmarking work.

## Testing

The Stage 1 correctness tests are registered with CTest.

Configure and build the Debug target:

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
camke --build build-debug
```

Run the tests:

```bash
ctest --test-dir build-debug --output-on-failure
```

The current test suite covers:

- Empty-book behavior
- Best bid and best ask
- Duplicate order-ID rejection
- Unknown-order cancellation
- Best-price updates after cancellation
- Empty price-level cleanup
- Invalid quantity rejection
- FIFO ordering within a price level

The current tests use C++ `assert`, so Debug builds are used for correctness testing. A more complete testing framework may be introduced in a later stage.

## Design

Stage 1 uses three primary data structures:

- `std::map` for sorted bid and ask price levels
- `std::list<Order>` for FIFO ordering within each price level
- `std::unordered_map<OrderId, OrderLocation>` as an order-ID index

The bid book is ordered from highest to lowest price, while the ask book is ordered from lowest to highest price. This makes the best bid and best ask available directly from the beginning of their respective maps.

Each price level stores orders in arrival order, preserving FIFO priority among orders at the same price.

The order-ID index stores the side, price, and list iterator for each resting order. This allows cancellation to locate an order without scanning every order at a price level. 

The current implementation deliberately prioritizes:

- Correctness
- Deterministic behavior
- Clear invariants
- Simple reasoning
- Maintainability

over:

- Cache locality
- Allocation efficiency
- Minimum latency
- Maximum throughput

Those performance trade-offs will be investigated in later stages rather than prematurely optimized.

See [`docs/order_book_design.md`](docs/order_book_design.md) for detailed Stage 1 design notes.

## Roadmap

### Stage 1 - Order Book

Implemented:

- Integer-tick price representation
- Bid and ask price levels
- Order insertion
- Order cancellation
- Best bid and best ask
- FIFO ordering within a price level
- Order-ID indexing
- Correctness tests

### Stage 2 - Deterministic Limit-Order Matching

Planned: 

- Limit-order crossing
- Price-time priority
- Trade generation
- Partial fills
- Full fills
- Multi-level sweeps
- FIFO execution within a price level
- Best bid and ask updates after matching
- Resting of unfilled quantity

The goal of Stage 2 is correctness and dterministic matching behavior rather than performance optimization.

### Stage 3 - Performance Analysis and Optimization

Planned:

- Microbenchmarks for core data structures
- Latency and throughput measurement
- Memory-allocation analysis
- Cache-locality investigation
- Comparison of alternative price-level representations
- Evaluation of alternatives to `std::map` and `std::list`
- Hot-path optimization

### Stage 4 - Event-Driven Architecture

Planned:

- Feed-handler integration
- Queue-based order ingestion
- Single-writer matching loop
- UDP networking experiments
- Event-look design
- Separation of networking, ingestion, and matching responsibilities

The longer-term architecture is expected to evolve toward:

```text
feed handler
    |
    v
ingestion queue
    |
    v
single-writer matching engine
    |
    v
trade / market-data output
```

## Development Philosophy

The project is intentionally developed in stages:

```text
correctness
    |
    v
deterministic matching
    |
    v
measurement
    |
    v
optimization
    |
    v
systems integration
```

Each stage is designed to establish correct behavior and clear invariants before introducing additional performance or concurrency complexity.