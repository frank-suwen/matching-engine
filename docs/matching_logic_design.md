# Matching Logic Design

## 1. Scope and Assumptions
- Single-threaded, deterministic matching.
- Limit orders only.
- Price represented as integer ticks.
- Quantity represented as singed integer.
- Nonpositive quantities are rejected.
- Duplicate active order IDs are rejected.
- Completed order IDs may be reused.

## 2. Price-Time Priority
- Incoming buys match the lowest ask first.
- incoming sells match the highest bid first.
- FIFO within each price level.
- Trades execute at the resting order's price.

## 3. Order Lifecycle
- Accepted incoming order
- Matching against resting liquidity
- Partial or full execution
- Resting unmatched remainder
- Cancellation
- Full-fill cleanup

## 4. Data Structures
- std::map for bid and ask price level
- std::list for FIFO resting orders
- std::unordered_map for active order-ID lookup

## 5. Correctness Invariants
- Price ordering
- FIFO time priority
- Order-ID index consistency
- Quantity conservation
- Uncrossed resting book

## 6. Exception Safety
- Local rollback for resting-order insertion
- No strong transaction guarantee for full submit_order()
- Allocation failures remain possible

## 7. Known Limitations
- No concurrency
- No market orders
- No order modification or replace
- No persistence or recovery
- No full transactional rollback
- No price-domain validation beyond the chosen tick type