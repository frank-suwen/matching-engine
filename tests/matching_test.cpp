#include "order_book.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace ob;

void assert_trade(
    const ob::Trade& trade,
    ob::OrderId aggressive_order_id,
    ob::OrderId passive_order_id,
    ob::Price price,
    ob::Quantity quantity
) {
    assert(trade.aggressive_order_id == aggressive_order_id);
    assert(trade.passive_order_id == passive_order_id);
    assert(trade.price == price);
    assert(trade.quantity == quantity);
}

ob::Quantity traded_quantity(const ob::MatchResult& result) {
    ob::Quantity total = 0;

    for (const auto& trade : result.trades) {
        total += trade.quantity;
    }

    return total;
}

// non-crossing buy rests
void test_buy_does_not_cross_ask() {
    OrderBook book;

    auto ask_result = book.submit_order(1, Side::Sell, 101, 20);
    
    assert(ask_result.has_value());
    assert(ask_result->trades.empty());
    assert(ask_result->resting_quantity == 20);

    auto buy_result = book.submit_order(10, Side::Buy, 100, 15);

    assert(buy_result.has_value());
    assert(buy_result->trades.empty());
    assert(buy_result->resting_quantity == 15);

    assert(book.best_bid() == 100);
    assert(book.best_ask() == 101);

    assert(book.order_quantity(1) == 20);
    assert(book.order_quantity(10) == 15);

    assert(traded_quantity(*buy_result) + buy_result->resting_quantity == 15);
}

// buy crosses ask with partial resting fill
void test_buy_crosses_ask_with_partial_resting_fill() {
    OrderBook book;

    auto ask_result = book.submit_order(1, Side::Sell, 100, 20);

    assert(ask_result.has_value());

    auto buy_result = book.submit_order(10, Side::Buy, 101, 15);

    assert(buy_result.has_value());
    assert(buy_result->trades.size() == 1);

    assert_trade(buy_result->trades[0], 10, 1, 100, 15);

    assert(buy_result->resting_quantity == 0);

    assert(book.order_quantity(1) == 5);
    assert(!book.order_quantity(10).has_value());

    assert(book.best_ask() == 100);
    assert(!book.best_bid().has_value());

    assert(traded_quantity(*buy_result) + buy_result->resting_quantity == 15);
}

// full fill removes passive order and price level
void test_full_fill_removes_resting_order() {
    OrderBook book;

    auto ask_result = book.submit_order(1, Side::Sell, 100, 10);

    assert(ask_result.has_value());

    auto buy_result = book.submit_order(10, Side::Buy, 100, 10);

    assert(buy_result.has_value());
    assert(buy_result->trades.size() == 1);

    assert_trade(buy_result->trades[0], 10, 1, 100, 10);

    assert(buy_result->resting_quantity == 0);

    assert(!book.order_quantity(1).has_value());
    assert(!book.order_quantity(10).has_value());

    assert(!book.best_ask().has_value());
    assert(!book.best_bid().has_value());

    assert(book.ask_level_count() == 0);
    assert(book.order_count() == 0);
}

// incoming remainder becomes resting liquidity
void test_incoming_remainder_rests() {
    OrderBook book;

    auto ask_result = book.submit_order(1, Side::Sell, 100, 8);

    assert(ask_result.has_value());

    auto buy_result = book.submit_order(10, Side::Buy, 100, 20);

    assert(buy_result.has_value());
    assert(buy_result->trades.size() == 1);

    assert_trade(buy_result->trades[0], 10, 1, 100, 8);

    assert(buy_result->resting_quantity == 12);

    assert(!book.order_quantity(1).has_value());
    assert(book.order_quantity(10) == 12);

    assert(!book.best_ask().has_value());
    assert(book.best_bid() == 100);

    assert(traded_quantity(*buy_result) + buy_result->resting_quantity == 20);
}

// FIFO priority at the same price
void test_fifo_priority_at_same_price() {
    OrderBook book;

    auto first = book.submit_order(1, Side::Sell, 100, 5);
    auto second = book.submit_order(2, Side::Sell, 100, 5);
    auto third = book.submit_order(3, Side::Sell, 100, 5);

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());

    auto buy_result = book.submit_order(10, Side::Buy, 100, 8);

    assert(buy_result.has_value());
    assert(buy_result->trades.size() == 2);

    assert_trade(buy_result->trades[0], 10, 1, 100, 5);

    assert_trade(buy_result->trades[1], 10, 2, 100, 3);

    assert(buy_result->resting_quantity == 0);

    assert(!book.order_quantity(1).has_value());
    assert(book.order_quantity(2) == 2);
    assert(book.order_quantity(3) == 5);

    const std::vector<OrderId> expected_ids{2, 3};

    assert(book.order_ids_at_price(Side::Sell, 100) == expected_ids);
}

// multi-level swap
void test_buy_sweeps_multiple_ask_levels() {
    OrderBook book;

    auto first = book.submit_order(1, Side::Sell, 100, 5);
    auto second = book.submit_order(2, Side::Sell, 101, 10);
    auto third = book.submit_order(3, Side::Sell, 102, 20);

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());

    auto buy_result = book.submit_order(10, Side::Buy, 101, 30);

    assert(buy_result.has_value());
    assert(buy_result->trades.size() == 2);

    assert_trade(buy_result->trades[0], 10, 1, 100, 5);

    assert_trade(buy_result->trades[1], 10, 2, 101, 10);

    assert(buy_result->resting_quantity == 15);

    assert(!book.order_quantity(1).has_value());
    assert(!book.order_quantity(2).has_value());

    assert(book.order_quantity(3) == 20);
    assert(book.order_quantity(10) == 15);

    assert(book.best_ask() == 102);
    assert(book.best_bid() == 101);

    assert(book.ask_level_count() == 1);
    assert(book.bid_level_count() == 1);

    assert(traded_quantity(*buy_result) + buy_result->resting_quantity == 30);
}

// duplicate rejection must not mutate the book
void test_duplicate_order_id_rejected_without_mutation() {
    OrderBook book;

    auto first = book.submit_order(1, Side::Buy, 100, 10);
    
    assert(first.has_value());

    auto duplicate = book.submit_order(1, Side::Buy, 99, 50);

    assert(!duplicate.has_value());

    assert(book.order_count() == 1);

    assert(book.best_bid() == 100);
    assert(!book.best_ask().has_value());

    assert(book.order_quantity(1) == 10);
    
    const std::vector<OrderId> expected_ids{1};

    assert(book.order_ids_at_price(Side::Buy, 100) == expected_ids);
}

int main() {
    // Stage 2 matching tests.
    test_buy_does_not_cross_ask();
    test_buy_crosses_ask_with_partial_resting_fill();
    test_full_fill_removes_resting_order();
    test_incoming_remainder_rests();
    test_fifo_priority_at_same_price();
    test_buy_sweeps_multiple_ask_levels();
    test_duplicate_order_id_rejected_without_mutation();

    std::cout << "All order book Stage 2 correctness tests passed.\n";
    return 0;
}