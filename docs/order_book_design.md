# Order Book Design Notes - Stage 1

## 1. Goal 

Stage 1 builds the core in-memory order book data structure.

Supported operations:

- Add order
- Cancel order
- Query best bid
- Query best ask
- Query order IDs at a price level for FIFO testing

This stage intentionally does **not** implement:

- Matching
- Market orders
- IOC orders
- Networking
- Concurrency

The goal of Stage 1 is correctness first.

# 2. Core Data Model

Prices are represented as integer ticks.

Example:

    using Price = std::int64_t

Instead of

    double price;

we represent prices as integer ticks.

Example:

    100.01 dollars
        ↓
    10001 ticks

Why?

- No floating-point precision problems
- Fast integer comparison
- Common representation in trading systems

---

# 3. Data Structures

The order book consists of three core containers.

Bid Book:

    std::map<Price, std::list<Order>, BidComparator>, bids_;

Ask Book:

    std::map<Price, std::list<Order>, AskComparator>, asks_;

Order index:

    std::unordered_map<OrderId, OrderLocation> orders_;

Each serves a different purpose;

## Bid Book

The bid side stores prices in descending order.

Highest price always comes first.

Example:

    10100
    10099
    10098

Therefore

    bid_.begin()->first

always returns the best bid.

---

## Ask Book

The ask side stores prices in ascending order.

Example:

    10101
    10102
    10103

Therefore

    asks_begin()->first

always returns the best ask.

---

## Price-level FIFO Queue

Each price level stores a FIFO queue using

    std::list<Order>

Example:

    Price 10000

    Order 1
    Order 2
    Order 3

The arrival order is preserved.

This matches the price-time priority used by most exchanges.

---

## Order ID Index

Every order is also stored in

    std::ordered_map<OrderId, OrderLocation>

OrderLocation stores:

- side
- price
- iterator into the linked list

Instead of searching every price level,

    cancel(order_id)

can immediately jump to the exact order

---

# 4. Why Store an Iterator?

The OrderLocation structure is

    struct OrderLocation {
        Side side;
        Price price;
        OrderList::iterator iterator;
    };

The iterator points directly to the order inside the FIFO list.

Without it:

To cancel order 123

you would need

    Search bid level
           ↓
    Search every order
           ↓
    Find order 
           ↓
    Erase

Time:

    O(number of orders at that price)

With the iterator:

    unordered_map lookup
           ↓
    iterator
           ↓
    erase(iterator)

The erase operation becomes

    O(1)

This is one of the most important design decisions in Stage 1.

---

# 5. Operation Complexity

## Add Order

Steps:

1. Validate quantity
2. Check duplicate ID
3. Find or create price level
4. Append order to FIFO queue
5. Save iterator in order index

Complexity

    O(log P)

where P is the number of price levels.

The map lookup dominates.

---

## Cancel Order

Steps:

1. Lookup order ID
2. Locate price level
3. Erase using stored iterator
4. Remove empty price level
5. Remove ID from unordered_map

Complexity:

    O(log P)

reason:

unordered_map lookup is average O(1)
m

map lookup is O(log P)

list erase by iterator is O(1)

---

## Best Bid

Implementation:

    bids_.begin()->first

Complexity:

    O(1)

---

## Best Ask

Implementation:

    asks_.begin()->first

Complexity:

    O(1)

---

# 6. Why std::map?

For Stage 1 we intentionally chose

    std::map

instead of more optimized structures.

Advantages:

- Automatically sorted
- Simple implementation
- Easy reasoning
- Stable iterators
- Great for correctness

Disadvantages:

- Tree structure
- Pointer chasing
- Poor cache locality
- Heap allocations

This is acceptable because Stage 1 prioritizes correctness.

Future stages will benchmark alternatives.

# 7. Cache and Performance Considerations

Current strengths:

- O(1) best bid
- O(1) best ask
- O(1) erase once iterator is known
- Deterministic integer prices
- FIFO semantics

Current weaknesses:

- std::map is pointer-heavy
- std::list is pointer-heavy
- Heap is allocation on insertion
- Cache misses during traversal

Future optimization candidates:

- boost::flat_map
- Sorted vector
- Price ladder
- Custom allocator
- Intrusive linked list
- Object pool

These optimzations are intentionally postponed.

---

# 8. Correctness Tests

Current tests verify:

✓ Empty book

✓ Best bid updates

✓ Best ask updates

✓ Duplicate order rejection

✓ Unknown cancel rejection

✓ Best price after cancellation

✓ Price level cleanup

✓ Invalid quantity rejection

✓ FIFO ordering

These tests verify the correctness of the data structure before introducing matching logic. 

---

# 9. Why Stage 1 Stops Here

A matching engine depends on a correct order book.

If

- add
- cancel 
- best bid
- best ask
- FIFO ordering

are incorrect,

then every future component will inherit those bugs.

Therefore Stage 1 intentionally focuses only on maintaining a correct order book.

---

# 10. Future Roadmap

## Stage 2

Matching Engine

Features:

- Limit order matching
- Partial fills
- Trade generation
- Market orders
- IOC orders
- Price-time priority

---

## Stage 3

Performance

Features:

- Benchmark throughput
- Measure latency
- Compare data structures
- Cache optimization
- Memory allocation optimization

--- 

## Stage 4

Infrastructure

Feature:

- UDP feed handler
- Market data replay
- Lock-free queues
- Event-driven architecture
- Multithreading

---

# 11. Final Takeaway

Stage 1 establishes a correct, deterministic, and maintainable order book.

The central design is:

    Sorted Price Levels
            + 
    FIFO Order Queues
            +
    O(1) Order Index

This foundation will support all future matching engine functionality.