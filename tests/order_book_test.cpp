#include "../include/order_book.hpp"

#include <cassert>
#include <iostream>

int main() {
    ob::OrderBook book;

    assert(book.order_count() == 0);
    assert(!book.best_bid().has_value());
    assert(!book.best_ask().has_value());

    assert(book.add_order(1, ob::Side::Buy, 10000, 10));
    assert(book.add_order(2, ob::Side::Buy, 10001, 20));
    assert(book.add_order(3, ob::Side::Sell, 10005, 15));
    assert(book.add_order(4, ob::Side::Sell, 10004, 25));

    assert(book.order_count() == 4);
    assert(book.best_bid().value() == 10001);
    assert(book.best_ask().value() == 10004);

    assert(!book.add_order(1, ob::Side::Buy, 10002, 5)); // duplicate id

    assert(book.cancel_order(2));
    assert(book.best_bid().value() == 10000);
    assert(book.order_count() == 3);

    assert(book.cancel_order(4));
    assert(book.best_ask().value() == 10005);
    assert(book.order_count() == 2);

    assert(!book.cancel_order(999)); // unknown id

    assert(book.cancel_order(1));
    assert(!book.best_bid().has_value());

    assert(book.cancel_order(3));
    assert(!book.best_ask().has_value());

    assert(book.order_count() == 0);

    std::cout << "All order book Stage 1 tests passed.\n";
    return 0;
}