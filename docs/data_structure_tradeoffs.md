# Data Structure Tradeoffs

## 1. Purpose

Stage 1 of the matching-engine project uses standard C++ containers to build a correct and deterministic in-memory order book.

The current implementation prioritizes clear semantics, price-time ordering, stable iterators, and straightforward cancellation over minimum latency and maximum cache efficiency.

This document examines the performance characteristics and tradeoffs of the current data structures and identifies alternatives that may be evaluated in later optimization stages.

---

## 2. Current Stage 1 Design

The order book currently uses three primary data structures:

```cpp
std::map<Price, OrderList, BidComparator> bids_;
std::map<Price, OrderList, AskComparator> asks_;
std::unordered_map<OrderId, OrderLocation> orders_;
```

Each price level stores orders using:

```cpp
using OrderList = std::list<Order>;
```

The responsibilities are separated as follows:

- `std::map` maintains sorted bid and ask price levels.
- `std::list` maintains FIFO order within each price level.
- `std::unordered_map` provides direct lookup from order ID to the order's location.

The `OrderLocation` structure stores the side, price, and list iterator for a resting order, allowing cancellation to locate the corresponding order without scanning the entire price level.

---

## 3. Performance Model

Big-O complexity describes how the number of operations grows with input size, but it does not fully describe execution cost on modern hardware.

Relevant hardware effects include:

- Cache-line utilization
- Spatial and temporal locality
- Pointer chasing
- Heap allocation and deallocation
- Branch behavior
- Working-set size
- Memory latency

Node-based containers such as `std::map` and `std::list` may require following pointers between objects stored at unrelated memory addresses. This can reduce spatial locality and increase the chance that accesses require data from slower levels of the memory hierarchy.

For this reason, two data structures with attractive asymptotic complexity may have substantially different real-world performance characteristics.

---

## 4. Microbenchmark Methodology

Small container microbenchmarks were run to connect algorithmic complexity with observed runtime behavior.

Environment:

- Linux development VM
- GCC 11.4.0
- C++20
- CMake Release build
- `std::chrono::steady_clock` for elapsed-time measurement

Tested container sizes:

- 1,000 elements
- 10,000 elements
- 100,000 elements

Lookup benchmarks performed 1,000,000 successful lookups per container size.

Each benchmark was run three times to observe basic run-to-run stability.

The reported values are average nanoseconds per operation and should be treated as workload-specific measurements rather than universal container latency.

---

## 5. Microbenchmark Results

Approximate observed results across repeated runs:

| Operation | N = 1,000 | N = 10,000 | N = 100,000 |
|---|---:|---:|---:|
| `std::map` insert | ~105–136 ns/op | ~146–149 ns/op | ~241–263 ns/op |
| `std::map` lookup | ~40–43 ns/op | ~70–73 ns/op | ~104–106 ns/op |
| `std::unordered_map` lookup | ~8.9–9.1 ns/op | ~9.1–9.3 ns/op | ~8.4–9.8 ns/op |
| `std::list::erase(iterator)` | ~13 ns/op | ~14 ns/op | ~13–18 ns/op |

The `std::map` lookup cost increased noticeably with container size, which is consistent with increased tree depth and a larger working set.

The `std::unordered_map` lookup cost remained relatively flat under this integer-key workload, consistent with its average constant-time lookup behavior.

The cost of `std::list::erase(iterator)` was also relatively insensitive to list size because the benchmark supplied the iterator directly and therefore did not require list traversal.

These measurements are intended as educational baselines. They do not establish universal performance ratios between the containers.

---

## 6. Price-Level Representation

The current Stage 1 implementation uses `std::map` to store bid and ask price levels.

This provides a general-purpose ordered structure with straightforward correctness properties, but it is not necessarily the most cache-efficient representation for a low-latency matching engine.

Potential alternatives include a flat map or a domain-specific price ladder.

### 6.1 `std::map`

The current implementation uses:

```cpp
std::map<Price, OrderList, BidComparator> bids_;
std::map<Price, OrderList, AskComparator> asks_;
```

A `std::map` is typically implemented as a balanced tree.

Conceptually:

```text
             10000
            /     \
         9998     10002
           \       /
          9999   10001
```

The main advantages are:

- Price levels remain automatically sorted.
- Insertion and lookup are `O(log P)`, where `P` is the number of active price levels.
- Best bid and best ask can be obtained directly from the beginning of the corresponding map.
- Empty price levels can be removed easily.
- The implementation is simple and easy to reason about.
- It is a standard C++ container with well-understood semantics.

These properties make `std::map` a strong correctness-first choice.

However, `std::map` is node-based. Tree nodes are generally allocated separately and may be located at unrelated memory addresses.

A lookup therefore involves pointer chasing:

```text
tree node
    |
    v
tree node
    |
    v
tree node
```

This can lead to:

- Poor spatial locality
- Multiple cache-line accesses
- Heap allocation on new price levels
- Branching during tree traversal
- Increasing lookup cost as the tree and working set grow

The Week 2 microbenchmark showed that `std::map` lookup cost increased noticeably as the number of stored elements increased.

This is consistent with the combination of increasing tree depth and a larger memory working set.

For Stage 1, these costs are acceptable because correctness and simple price ordering are more important than minimum latency.

### 6.2 Flat Map

A flat-map-style structure stores sorted key-value pairs in contiguous or mostly contiguous memory.

Conceptually:

```text
[9998][9999][10000][10001][10002]
```

rather than:

```text
        node
       /    \
    node    node
```

Potential advantages include:

- Better spatial locality
- Fewer pointer dereferences
- Fewer per-element heap allocations
- Better cache-line utilization
- More predictable sequential traversal
- Efficient binary-search lookup

Because elements are stored close together in memory, loading one cache line may bring multiple nearby price levels into the cache.

This can make a flat representation faster in practice even though lookup is still typically `O(log P)`.

However, insertion is more expensive.

If a new price level must be inserted into the middle of the sorted sequence:

```text
Before:

[9998][9999][10001][10002]

Insert 10000

After:

[9998][9999][10000][10001][10002]
```

existing elements may need to be shifted.

Therefore insertion can be `O(P)` in the worst case.

The main tradeoff is:

```text
std::map

better insertion complexity
worse locality

        versus

flat map

worse insertion complexity
better locality
```

Which design performs better depends on the workload.

For example, a flat representation may be attractive if the number of active price levels is relatively small or if traversal and lookup dominate price-level creation and deletion.

A future performance stage could benchmark a flat-map implementation against the current `std::map` baseline.

### 6.3 Price Ladder

A price ladder uses knowledge about the trading domain rather than relying on a general-purpose ordered container.

If prices are represented as integer ticks and the valid price range is known or bounded, a price can potentially be converted directly into an array index.

For example:

```text
minimum price = 10000

price 10000 -> index 0
price 10001 -> index 1
price 10002 -> index 2
price 10003 -> index 3
```

The structure could conceptually look like:

```text
index
  0       1       2       3       4
  |       |       |       |       |
  v       v       v       v       v

[Level][Empty][Level][Level][Empty]

10000  10001  10002  10003  10004
```

Price-level access can then become approximately:

```text
index = price - minimum_price
```

followed by direct array access.

Potential advantages include:

- Direct `O(1)` price indexing
- Strong spatial locality
- Fewer pointer dereferences
- Potentially fewer allocations
- Predictable memory layout
- Good compatibility with cache-aware optimization

A price ladder can therefore be very attractive for a low-latency order book.

However, it relies on assumptions about the price domain.

Important design questions include:

- What is the minimum supported price?
- What is the maximum supported price?
- How large is the valid tick range?
- How sparse are active price levels?
- How much memory would unused levels consume?
- How is the next occupied bid or ask level found efficiently?
- How should different instruments and tick sizes be handled?

A naive price ladder may provide fast direct access but waste substantial memory if the possible price range is large and sparsely populated.

Finding the best occupied level can also require additional mechanisms such as:

- Tracking the current best price directly
- Bitsets for occupied price levels
- Hierarchical bitmaps
- Other indexing structures

Therefore a price ladder is not simply a faster replacement for `std::map`; it is a more specialized design that trades generality for predictable memory access and potentially lower latency.

### 6.4 Stage 1 Decision

Stage 1 keeps `std::map` because it provides the required semantics with minimal implementation complexity:

```text
sorted prices
    +
simple insertion/removal
    +
direct best-price access
    +
clear correctness
```

The alternative structures introduce additional complexity and workload assumptions that are not necessary before deterministic matching is implemented.

The intended progression is:

```text
Stage 1
correct ordered representation
        |
        v
Stage 2
correct deterministic matching
        |
        v
Stage 3
benchmark real matching workload
        |
        v
evaluate flat map / price ladder
```

The current `std::map` design should therefore be viewed as a correctness-first baseline rather than the final low-latency price-level representation.

---

## 7. FIFO Queue Representation

Each price level in the current Stage 1 implementation stores resting orders using:

```cpp
using OrderList = std::list<Order>;
```

The primary requirements are:

- Preserve FIFO arrival order within a price level
- Append new resting orders efficiently
- Remove an arbitrary resting order efficiently when a cancellation arrives
- Keep references or iterators to existing orders valid when unrelated orders are inserted or removed

`std::list` satisfies these requirements cleanly, which makes it a strong correctness-first choice.

However, matching workloads also require sequential traversal through resting liquidity, so cache locality and memory layout become important performance considerations.

Potential alternatives include `std::deque` and intrusive linked structures.

### 7.1 `std::list`

A `std::list` is a node-based doubly linked structure.

Conceptually:

```text
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│ prev         │      │ prev         │      │ prev         │
│ Order 1      │ ---> │ Order 2      │ ---> │ Order 3      │
│ next         │      │ next         │      │ next         │
└──────────────┘      └──────────────┘      └──────────────┘
```

The main advantages are:

- `O(1)` insertion at the back
- `O(1)` erase when an iterator is already known
- Stable iterators to non-erased elements
- Natural FIFO ordering
- Simple cancellation semantics
- Straightforward standard-library ownership

These properties fit the Stage 1 order-ID index well.

The index stores:

```cpp
struct OrderLocation {
    Side side;
    Price price;
    OrderList::iterator iterator;
};
```

Cancellation can therefore:

```text
order ID
    |
    v
OrderLocation
    |
    v
known list iterator
    |
    v
erase(iterator)
```

without scanning through all orders at the price level.

The Week 2 microbenchmark showed that `std::list::erase(iterator)` remained relatively insensitive to container size when the iterator was already known.

This is consistent with its constant-time erase semantics.

However, `std::list` has several performance disadvantages.

Each node is generally stored separately in memory, so traversing:

```text
Order 1
   |
   v
Order 2
   |
   v
Order 3
```

requires following pointers between nodes.

The nodes may occupy unrelated memory locations.

Potential costs include:

- Poor spatial locality
- Frequent pointer chasing
- One allocation per node
- Extra memory for `prev` and `next` pointers
- More cache-line accesses during sequential traversal
- Less effective hardware prefetching

These limitations may become important during Stage 2 matching, because matching often walks orders at the front of one or more price levels sequentially.

Although linked-list traversal is algorithmically `O(K)` for `K` visited orders, the constant cost of each step may be relatively high because each next node can require another pointer-dependent memory access.

Therefore `std::list` provides strong mutation semantics but weak cache locality.

### 7.2 `std::deque`

A `std::deque` provides FIFO-style insertion and removal while storing elements in blocks of contiguous memory rather than one independently allocated node per element.

Conceptually:

```text
Block 1                    Block 2

[Order][Order][Order]      [Order][Order][Order]
```

The entire deque is not guaranteed to be one contiguous allocation like a `std::vector`, but neighboring elements are generally grouped into contiguous blocks.

Potential advantages include:

- Better spatial locality than `std::list`
- Fewer individual allocations
- Efficient push at the back
- Efficient removal from the front
- Efficient sequential iteration
- Natural support for queue-like workloads

These properties are attractive for matching.

A matching engine frequently processes resting orders from the front of a price level:

```text
front
 |
 v
[Order 1][Order 2][Order 3][Order 4]
```

If multiple nearby orders occupy the same cache line or nearby cache lines, traversal can be more cache-friendly than following separately allocated list nodes.

However, `std::deque` does not provide the same arbitrary-erasure characteristics as `std::list`.

Removing an element from the middle of a deque may require moving other elements.

Iterator invalidation rules are also more complicated.

This creates tension with the Stage 1 cancellation design.

The tradeoff therefore becomes:

```text
std::list

strong arbitrary erase semantics
stable iterators
poor locality

        versus

std::deque

better locality
good front/back operations
more difficult arbitrary cancellation
```

A deque may be attractive if the matching workload is dominated by FIFO front processing and if cancellation can be supported through a different indexing strategy.

### 7.3 Intrusive List

An intrusive list embeds the list linkage directly into the order object rather than having a standard container own a separate node abstraction.

Conceptually:

```cpp
struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;

    Order* prev;
    Order* next;
};
```

The exact implementation may instead use intrusive hooks provided by a library, but the central idea is that the order itself participates directly in the linked structure.

Potential advantages include:

- More explicit control over memory layout
- Ability to allocate orders from an object pool
- Stable object addresses
- `O(1)` insertion and removal
- Potentially more predictable allocation behavior
- Better integration with specialized low-latency memory management

However, intrusive structures add significant implementation complexity.

The programmer must reason carefully about:

- Object lifetime
- Ownership
- Removal from lists
- Reuse of order objects
- Pointer correctness
- Interaction with the order-ID index
- Avoiding dangling pointers

Therefore an intrusive list is better viewed as a later optimization candidate rather than a Stage 1 replacement.

### 7.4 Cancellation vs Matching Tradeoff

Cancellation benefits from:

```text
order ID
    |
    v
direct location
    |
    v
erase arbitrary order
```

Matching benefits from:

```text
front order
    |
    v
next order
    |
    v
next order
```

The former favors stable direct locations and constant-time unlinking.

The latter favors contiguous or near-contiguous memory and efficient sequential traversal.

The optimal representation therefore depends on the actual workload, including:

- Cancellation frequency
- Average orders per price level
- Typical sweep depth
- Frequency of partial fills
- Allocation behavior
- Cache-miss cost

These workload characteristics should be measured before changing the representation.

### 7.5 Stage 1 Decision

Stage 1 keeps `std::list<Order>` because it provides:

```text
FIFO ordering
    +
stable iterators
    +
O(1) erase by iterator
    +
simple ownership
    +
clear correctness
```

Its cache-efficiency limitations are accepted intentionally.

The intended progression is:

```text
Stage 1
std::list for correctness
        |
        v
Stage 2
implement deterministic matching
        |
        v
Stage 3
measure actual traversal and cancellation workload
        |
        v
evaluate deque / intrusive structure / object pool
```

The current list-based design should therefore be treated as a correctness-first baseline rather than the final low-latency FIFO representation.

---

## 8. Order-ID Index Design

The Stage 1 order book maintains a secondary index from each order ID to the order's location inside the book:

```cpp
std::unordered_map<OrderId, OrderLocation> orders_;
```

where:

```cpp
struct OrderLocation {
    Side side;
    Price price;
    OrderList::iterator iterator;
};
```

This index is not the primary owner of the `Order` objects.

The actual orders are owned by the FIFO lists stored at each price level.

The order-ID index stores enough information to locate an already-resting order efficiently.

### 8.1 Why an Order-ID Index Is Needed

The primary order-book representation is organized around:

```text
price
    |
    v
price level
    |
    v
FIFO orders
```

Cancellation begins with:

```text
order ID
```

Without a secondary index, cancelling an arbitrary order could require scanning through the order book.

The order-ID index instead provides:

```text
OrderId
   |
   v
hash lookup
   |
   v
OrderLocation
```

Different access patterns may justify maintaining different indexes over the same logical data.

### 8.2 Cancellation Path

The current cancellation path conceptually performs:

```text
1. Lookup order ID
        |
        v
2. Retrieve OrderLocation
        |
        +--> side
        +--> price
        +--> iterator
        |
        v
3. Find corresponding price level
        |
        v
4. Erase order using stored iterator
        |
        v
5. Remove empty price level if necessary
        |
        v
6. Remove order-ID index entry
```

The important operations are approximately:

```text
orders_.find(id)
    average O(1)

bids_.find(price) / asks_.find(price)
    O(log P)

list.erase(iterator)
    O(1)
```

where `P` is the number of active price levels.

The index therefore avoids a linear scan through the FIFO queue when cancelling an order.

### 8.3 Why `std::unordered_map`

Order IDs do not require sorted traversal.

The important question is:

```text
"Given this exact order ID, where is the order?"
```

Therefore a hash-based associative container is a natural Stage 1 choice.

The Week 2 benchmark also showed relatively flat successful integer-key lookup behavior for `std::unordered_map` across the tested sizes.

The benchmark does not establish a universal performance ratio, but it is consistent with using a hash-based structure when exact keyed lookup matters and ordering does not.

### 8.4 Why `OrderLocation` Stores Side, Price, and Iterator

The side identifies whether the order belongs to the bid or ask book.

The price identifies the containing price level.

The iterator identifies the exact order within the FIFO queue.

Together:

```text
side
  |
  v
bid or ask book

price
  |
  v
price level

iterator
  |
  v
exact order
```

provide the complete structural location.

### 8.5 Representation Invariants

For every resting order:

- Exactly one order-ID index entry must exist.
- Every index entry must refer to a currently resting order.
- The stored side must match the book containing the order.
- The stored price must match the price level containing the order.
- The stored iterator must point to the corresponding order.
- A removed or cancelled order must have no remaining index entry.
- Duplicate active order IDs must be rejected.

These invariants become especially important during Stage 2 matching.

### 8.6 Stage 2 Implications

For a partial fill:

```text
same OrderId
same side
same price
same iterator
updated quantity only
```

so the existing `OrderLocation` can remain valid.

For a full fill, the engine must remove both:

```text
list node
    +
order-ID index entry
```

and potentially remove the now-empty price level.

Failure to remove the index entry could leave a stale iterator and lead to incorrect behavior or undefined behavior.

### 8.7 Cache-Efficiency Limitations

Although `std::unordered_map` provides attractive average lookup complexity, it is not necessarily the final low-latency representation.

Potential costs include:

- Hash computation
- Bucket-array access
- Pointer chasing
- Per-node allocation
- Cache misses
- Memory overhead
- Rehashing during growth

Potential future alternatives include:

- Open-addressing hash tables
- Robin Hood hashing
- Dense hash tables
- Fixed-capacity hash tables
- Direct indexing when suitable constraints exist

These alternatives should be evaluated through measurement rather than adopted solely because they appear more cache-friendly.

### 8.8 Stage 1 Decision

Stage 1 keeps:

```cpp
std::unordered_map<OrderId, OrderLocation> orders_;
```

because it provides:

```text
simple keyed lookup
        +
average O(1) complexity
        +
standard-library semantics
        +
easy integration with stored list iterators
```

The current order-ID index should therefore be treated as a correctness-first secondary index rather than the final low-latency implementation.

---

## 9. Cache-Efficiency Limitations of Stage 1

The Stage 1 order book deliberately uses standard node-based containers.

These structures provide clear semantics and convenient correctness properties, but the combined representation is not optimized for cache locality or minimum memory-access latency.

The main limitations include:

- Pointer chasing
- Scattered heap allocations
- Poor spatial locality
- Multiple container lookups for one logical operation
- Indirect access between the order-ID index and price-level queues
- Limited control over memory placement

### 9.1 Logical Structure vs Physical Memory Layout

Logically:

```text
OrderId
   |
   v
OrderLocation
   |
   v
Price Level
   |
   v
Order
```

Physically, cancellation may involve:

```text
unordered_map bucket
        |
        v
unordered_map node
        |
        v
std::map tree node
        |
        v
std::list node
        |
        v
Order
```

These objects are not necessarily adjacent in memory.

Each pointer dereference may therefore lead to another cache line.

### 9.2 Cache Lines and Spatial Locality

If useful neighboring data is stored together:

```text
[Order 1][Order 2][Order 3][Order 4]
```

loading one cache line may make additional useful data available.

A linked structure may instead place logically adjacent objects at unrelated addresses:

```text
0x1000 -> Order 1
0x8400 -> Order 2
0x2A000 -> Order 3
```

Sequential traversal can therefore require additional cache-line accesses.

### 9.3 Pointer Chasing

Pointer chasing occurs when the address of the next access depends on data loaded by the current access.

For example:

```text
load node
   |
   v
read pointer
   |
   v
load next node
```

Both tree traversal and linked-list traversal can create chains of dependent memory accesses.

This can increase effective memory latency.

### 9.4 `std::map` Cache Behavior

Price-level lookup may involve multiple tree-node accesses.

Potential costs include:

- Dependent pointer loads
- Branch decisions
- Multiple cache-line accesses
- Per-price-level allocation

The Week 2 benchmark showed increasing `std::map` lookup latency as the tested container size increased.

This is consistent with increasing tree depth and a larger working set, although the benchmark does not directly isolate cache misses.

### 9.5 `std::list` Cache Behavior

`std::list` provides stable iterators and constant-time erase when the iterator is already known.

However, matching may need to traverse:

```text
Order 1
   |
   v
Order 2
   |
   v
Order 3
```

and those nodes may not be physically adjacent.

Potential costs include:

- Poor spatial locality
- Dependent pointer loads
- Per-order allocation
- Extra linkage memory

The `erase(iterator)` microbenchmark measured direct removal rather than sequential traversal.

### 9.6 `std::unordered_map` Cache Behavior

A typical order-ID lookup may involve:

```text
OrderId
   |
   v
hash
   |
   v
bucket array
   |
   v
entry / node
```

Average `O(1)` complexity does not imply a single memory access or perfect locality.

The Week 2 benchmark used a favorable workload consisting of reserved capacity, integer keys, and successful lookups.

Real order-flow patterns may behave differently.

### 9.7 Composite Cost of `cancel_order`

Conceptually:

```text
cancel(order_id)
      |
      v
unordered_map lookup
      |
      v
OrderLocation
      |
      v
std::map lookup
      |
      v
std::list erase(iterator)
      |
      v
unordered_map erase
```

Algorithmically:

```text
average O(1)
    +
O(log P)
    +
O(1)
```

is attractive.

Physically, however, the operation may touch hash-table memory, tree-node memory, and list-node memory at unrelated addresses.

An operation can therefore be algorithmically efficient while still having nontrivial memory-access latency.

### 9.8 Composite Cost of `add_order`

Adding an order may involve:

```text
duplicate-ID lookup
        +
price-level tree traversal
        +
possible map-node allocation
        +
list-node allocation
        +
order-ID index insertion
```

The actual hardware cost therefore depends on more than asymptotic complexity.

Future optimization work should benchmark complete order-book operations rather than relying only on isolated STL measurements.

### 9.9 Allocation Overhead

The current representation may dynamically allocate:

```text
new price-level node
        +
new list node
        +
new hash-table entry
```

Dynamic allocation can introduce:

- Allocator bookkeeping
- Fragmentation
- Unpredictable memory placement
- Cache and TLB effects
- Latency variability

Later stages may investigate:

- Object pools
- Arena allocation
- Custom allocators
- Preallocated storage
- Intrusive containers

### 9.10 Temporal Locality

Some parts of the book may naturally remain hot:

```text
best bid
best ask
front order at best price
```

while distant price levels and rarely accessed orders may remain cold.

Future benchmarks should eventually consider workloads such as:

- Repeated access to best prices
- Deep-book lookups
- Bursty cancellation
- Multi-level sweeps
- Hot and cold order IDs

### 9.11 NUMA Considerations

NUMA becomes relevant when memory-access latency depends on which CPU socket or NUMA node owns the memory.

A future architecture may resemble:

```text
network / feed thread
        |
        v
queue
        |
        v
matching thread
```

However, NUMA optimization is not currently a meaningful Stage 1 priority because the project is small, single-threaded, and running in a development environment.

The relevant lesson is that memory placement can eventually matter in addition to data-structure choice.

### 9.12 Why These Limitations Are Acceptable

The current representation intentionally prioritizes:

```text
clear ownership
        +
simple price ordering
        +
stable FIFO semantics
        +
direct cancellation
        +
standard-library safety
```

These properties allow deterministic matching to be implemented without simultaneously debugging custom memory-management systems.

The performance limitations are known rather than accidental.

---

## 10. Why Stage 1 Is Correctness-First

Stage 1 deliberately uses standard C++ containers even though their cache and allocation characteristics are not ideal for a final low-latency matching engine.

The goal is to establish a correct and understandable order-book model before introducing specialized data structures or memory-management techniques.

The current representation prioritizes:

- Correct price ordering
- FIFO semantics within each price level
- Reliable cancellation
- Stable references to resting orders
- Clear ownership
- Simple invariants
- Maintainable tests
- Straightforward debugging

### 10.1 Correctness Before Optimization

A matching engine depends on interacting invariants:

```text
best bid / ask ordering
        +
FIFO priority
        +
order-ID uniqueness
        +
correct cancellation
        +
price-level cleanup
        +
consistent indexes
```

If the underlying order book is incorrect, future matching behavior will also be incorrect regardless of performance.

The current containers provide clear semantics:

```text
std::map
    -> sorted price levels

std::list
    -> FIFO ordering + stable iterators

std::unordered_map
    -> direct order-ID lookup
```

### 10.2 Why Premature Optimization Would Increase Risk

Specialized structures introduce additional correctness concerns.

A custom price ladder may require reasoning about price bounds, sparse levels, best-price tracking, and occupancy.

An intrusive queue may require manual lifetime management, pointer validity, object reuse, and pool ownership.

A custom hash table may require collision handling, deletion, resizing, and probing behavior.

Introducing all of these concerns before Stage 2 would make it harder to determine whether a bug originates from matching logic or from the custom data structures.

### 10.3 Standard Containers as a Baseline

The STL implementation serves as both:

```text
correct reference implementation
        +
performance baseline
```

Future implementations can be compared against it using the same correctness tests and benchmark workloads.

### 10.4 Measurement Before Replacement

The Week 2 microbenchmarks reveal useful characteristics of the individual containers, but they do not justify replacing them immediately.

Real matching-engine performance depends on composed operations such as:

```text
add order
cancel order
partial fill
full fill
single-level match
multi-level sweep
```

The intended optimization process is:

```text
build correct matching behavior
        |
        v
measure full-engine hot paths
        |
        v
identify actual bottlenecks
        |
        v
replace specific structures
        |
        v
measure again
```

### 10.5 Determinism Before Concurrency

Stage 1 and Stage 2 intentionally avoid introducing concurrency into the matching core.

The future architecture can instead place concurrency around a single-writer matching engine:

```text
network / feed handling
        |
        v
queue
        |
        v
single-writer matching engine
```

This preserves deterministic book mutation while allowing surrounding components to operate concurrently.

### 10.6 Optimization Should Preserve Semantics

Future data-structure changes should modify implementation rather than behavior.

Replacing a `std::map` with a price ladder must not change:

- Best bid and ask semantics
- Price priority
- FIFO priority
- Matching results

Likewise, replacing `std::list` with another FIFO representation must not change cancellation or time-priority behavior.

Stage 1 and Stage 2 tests therefore become an important regression suite for future optimization.

### 10.7 Engineering Progression

The project intentionally follows:

```text
Stage 1
correct order-book data structure
        |
        v
Stage 2
correct deterministic matching
        |
        v
Stage 3
measurement and profiling
        |
        v
targeted optimization
        |
        v
Stage 4
event-driven systems integration
```

### 10.8 Stage 1 Decision

The Stage 1 implementation is intentionally not the final low-latency representation.

It is a reference implementation emphasizing:

```text
correctness
    +
clarity
    +
determinism
    +
testability
```

while documenting known limitations involving:

```text
pointer chasing
heap allocation
cache locality
memory layout
```

---

## 11. Future Optimization Plan

The Stage 1 order book provides a correctness-first baseline using standard C++ containers.

Potential performance limitations have been identified in:

- Price-level lookup
- FIFO order traversal
- Order-ID lookup
- Dynamic allocation
- Cache locality
- Pointer chasing

However, these observations do not by themselves justify replacing the current data structures.

Future optimization should be driven by measurements of realistic matching-engine operations.

### 11.1 Establish the Stage 2 Baseline

Before changing the underlying data structures, Stage 2 should implement a complete deterministic limit-order matching path.

The baseline workload should support:

- Non-crossing orders that rest on the book
- Crossing buy and sell orders
- Partial fills
- Full fills
- FIFO execution within a price level
- Multi-order matching
- Multi-level sweeps
- Correct best bid and ask updates
- Correct order-ID index maintenance

This provides a realistic workload for later performance analysis.

### 11.2 Benchmark Full Engine Operations

After Stage 2 is complete, benchmarks should move beyond isolated container operations.

Important operations include:

```text
add non-crossing order
cancel resting order
match one resting order
match several orders at one price
sweep multiple price levels
partially fill a resting order
fully consume a price level
```

Measurements should initially include:

- Throughput
- Average latency
- p50 latency
- p99 latency

Where practical, later measurements may also examine:

- Cache misses
- Branch behavior
- Allocation frequency
- Instructions per operation

### 11.3 Profile Before Replacing

The optimization process should follow:

```text
measure
   |
   v
identify bottleneck
   |
   v
form hypothesis
   |
   v
change one component
   |
   v
measure again
   |
   v
verify correctness
```

Changing one major component at a time makes the effect of each optimization easier to understand.

### 11.4 Price-Level Optimization Candidates

Potential alternatives to the current `std::map` include:

- Flat-map-style sorted storage
- Dense price ladder
- Sparse or segmented price ladder
- Price ladder with occupancy metadata

Evaluation should consider:

- Finding the best price
- Finding the next occupied price
- Creating a price level
- Removing an empty price level
- Traversing multiple levels during a sweep
- Memory usage for sparse books

### 11.5 FIFO Queue Optimization Candidates

Potential alternatives to `std::list<Order>` include:

- `std::deque`
- Intrusive linked structures
- Preallocated order nodes
- Object pools
- Specialized queue representations

The central tradeoff remains:

```text
fast arbitrary cancellation
        versus
cache-efficient sequential matching
```

Both matching and cancellation workloads should be measured before changing the representation.

### 11.6 Order-ID Index Optimization Candidates

Potential alternatives to `std::unordered_map` include:

- Open-addressing hash tables
- Robin Hood hashing
- Dense hash tables
- Fixed-capacity hash tables
- Direct indexing when suitable constraints exist

Potential goals include:

- Reduced pointer chasing
- Better cache locality
- Fewer per-entry allocations
- More predictable latency

### 11.7 Memory-Management Optimization

Later stages may investigate:

- Preallocated memory
- Object pools
- Arena allocation
- Custom allocators
- Capacity reservation

The objective would be to reduce:

- Hot-path allocation
- Heap fragmentation
- Unpredictable memory placement
- Allocation-related latency variance

Memory-management changes must preserve clear ownership and lifetime rules.

### 11.8 Preserve Correctness During Optimization

Every optimization must preserve:

- Price priority
- FIFO time priority
- Deterministic execution
- Correct partial fills
- Correct full fills
- Correct cancellation
- Correct best bid and ask
- Correct order-ID index state

Conceptually:

```text
same input order stream
        |
        +----------------------+
        |                      |
        v                      v
baseline engine          optimized engine
        |                      |
        +----------+-----------+
                   |
                   v
          same observable result
```

The correctness test suite therefore becomes a regression suite for performance optimization.

### 11.9 Optimization Priority

A reasonable future sequence is:

```text
1. Complete deterministic matching
        |
        v
2. Build realistic engine benchmarks
        |
        v
3. Profile full hot paths
        |
        v
4. Reduce demonstrated allocation overhead
        |
        v
5. Evaluate price-level representation
        |
        v
6. Evaluate FIFO order representation
        |
        v
7. Evaluate order-ID index
        |
        v
8. Re-measure complete engine
```

This sequence is provisional.

Profiling results should determine the actual optimization priority.

### 11.10 Optimization Philosophy

The project follows three principles:

**Correctness before optimization**

The engine must produce correct deterministic results before performance changes are introduced.

**Measurement before replacement**

A theoretically attractive data structure should not replace a working implementation without evidence that it improves the relevant workload.

**Optimize the complete operation**

Performance decisions should consider full matching-engine paths rather than isolated container operations.

The overall progression is:

```text
correctness
    |
    v
determinism
    |
    v
measurement
    |
    v
profiling
    |
    v
targeted optimization
    |
    v
measurement again