#include "order_book.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

ob::Quantity traded_quantity(const ob::MatchResult& result) {
    ob::Quantity total = 0;

    for (const auto& trade : result.trades) {
        total += trade.quantity;
    }

    return total;
}

void test_empty_book() {
    ob::OrderBook book;

    assert(book.order_count() == 0);
    assert(book.bid_level_count() == 0);
    assert(book.ask_level_count() == 0);
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());
}

void submit_and_expect_resting(
    ob::OrderBook& book,
    ob::OrderId id,
    ob::Side side,
    ob::Price price,
    ob::Quantity quantity
) {
    auto result = book.submit_order(id, side, price, quantity);

    assert(result.has_value());
    assert(result->trades.empty());
    assert(result->resting_quantity == quantity);
}

void cancel_and_expect_success(
    ob::OrderBook& book,
    ob::OrderId id
) {
    const bool cancelled = book.cancel_order(id);
    assert(cancelled);
}

void test_best_bid_and_ask() {
    ob::OrderBook book;

    submit_and_expect_resting(book, 1, ob::Side::Buy, 10000, 10);
    submit_and_expect_resting(book, 2, ob::Side::Buy, 10001, 20);
    submit_and_expect_resting(book, 3, ob::Side::Sell, 10005, 15);
    submit_and_expect_resting(book, 4, ob::Side::Sell, 10004, 25);

    assert(book.best_bid().value() == 10001);
    assert(book.best_ask().value() == 10004);

    assert(book.order_count() == 4);
    assert(book.bid_level_count() == 2);
    assert(book.ask_level_count() == 2);
}

void test_duplicate_order_id_rejected() {
    ob::OrderBook book;
    
    submit_and_expect_resting(book, 1, ob::Side::Buy, 10000, 10);

    auto duplicate = book.submit_order(1, ob::Side::Sell, 9999, 20);
    assert(!duplicate.has_value());

    assert(book.order_count() == 1);
    assert(book.best_bid().value() == 10000);
    assert(!book.best_ask().has_value());
}

void test_cancel_unknown_order() {
    ob::OrderBook book;

    const bool cancelled = book.cancel_order(999);
    assert(!cancelled);
    assert(book.order_count() == 0);
}

void test_cancel_updates_best_price() {
    ob::OrderBook book;

    submit_and_expect_resting(book, 1, ob::Side::Buy, 10000, 10);
    submit_and_expect_resting(book, 2, ob::Side::Buy, 10002, 10);
    submit_and_expect_resting(book, 3, ob::Side::Buy, 10001, 10);

    assert(book.best_bid().value() == 10002);

    cancel_and_expect_success(book, 2);
    assert(book.best_bid().value() == 10001);

    cancel_and_expect_success(book, 3);
    assert(book.best_bid().value() == 10000);

    cancel_and_expect_success(book, 1);
    assert(!book.best_bid().has_value());
}

void test_price_level_cleanup() {
    ob::OrderBook book;

    submit_and_expect_resting(book, 1, ob::Side::Buy, 10000, 10);
    submit_and_expect_resting(book, 2, ob::Side::Buy, 10000, 20);

    assert(book.bid_level_count() == 1);
    
    cancel_and_expect_success(book, 1);
    assert(book.bid_level_count() == 1);

    cancel_and_expect_success(book, 2);
    assert(book.bid_level_count() == 0);

    assert(!book.best_bid().has_value());
}

void test_invalid_quantity_throws() {
    ob::OrderBook book;

    bool threw = false;
    try {
        book.submit_order(1, ob::Side::Buy, 10000, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }

    assert(threw);
    assert(book.order_count() == 0);
}

void test_fifo_order_at_price_level() {
    ob::OrderBook book;
    
    submit_and_expect_resting(book, 1, ob::Side::Buy, 10000, 10);
    submit_and_expect_resting(book, 2, ob::Side::Buy, 10000, 20);
    submit_and_expect_resting(book, 3, ob::Side::Buy, 10000, 30);

    std::vector<ob::OrderId> ids = book.order_ids_at_price(ob::Side::Buy, 10000);

    assert((ids == std::vector<ob::OrderId>{1, 2, 3}));

    cancel_and_expect_success(book, 2);

    ids = book.order_ids_at_price(ob::Side::Buy, 10000);

    assert((ids == std::vector<ob::OrderId>{1, 3}));
}

int main() {
    // Stage 1 tests.
    test_empty_book();
    test_best_bid_and_ask();
    test_duplicate_order_id_rejected();
    test_cancel_unknown_order();
    test_cancel_updates_best_price();
    test_price_level_cleanup();
    test_invalid_quantity_throws();
    test_fifo_order_at_price_level();

    std::cout << "All order book Stage 1 correctness tests passed.\n";
    return 0;
}