#pragma once 

#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ob {

using OrderId = std::uint64_t;
using Price = std::int64_t;       // integer ticks, not double
using Quantity = std::int64_t;

enum class Side {
    Buy,
    Sell
};

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity quantity;
};

struct Trade {
    OrderId aggressive_order_id;
    OrderId passive_order_id;
    Price price;
    Quantity quantity;
};

struct MatchResult {
    std::vector<Trade> trades;
    Quantity resting_quantity;
};

struct BidComparator {
    bool operator()(Price lhs, Price rhs) const {
        return lhs > rhs; // highest bid first
    }
};

struct AskComparator {
    bool operator()(Price lhs, Price rhs) const {
        return lhs < rhs; // lowest ask first
    }
};

class OrderBook {
private:
    using OrderList = std::list<Order>;
    using BidBook = std::map<Price, OrderList, BidComparator>;
    using AskBook = std::map<Price, OrderList, AskComparator>;

    struct OrderLocation {
        Side side;
        Price price;
        OrderList::iterator iterator;
    };

    bool crosses(Side side, Price price) const {
        if (side == Side::Buy) {
            return !asks_.empty() && price >= asks_.begin()->first;
        }

        return !bids_.empty() && price <= bids_.begin()->first;
    }

public:
    bool add_order(OrderId id, Side side, Price price, Quantity quantity) {
        if (quantity <= 0) {
            throw std::invalid_argument("quantity must be positive");
        }

        if (orders_.contains(id)) {
            return false; // duplicate order id
        }

        if (side == Side::Buy) {
            auto& level = bids_[price];
            level.push_back(Order{id, side, price, quantity});
            auto it = std::prev(level.end());
            orders_.emplace(id, OrderLocation{side, price, it});
        } else {
            auto& level = asks_[price];
            level.push_back(Order{id, side, price, quantity});
            auto it = std::prev(level.end());
            orders_.emplace(id, OrderLocation{side, price, it});
        }

        return true;
    }

    std::optional<MatchResult> submit_order(
        OrderId id,
        Side side,
        Price price,
        Quantity quantity
    ) {
        // 1. Validate quantity.
        if (quantity <= 0) {
            throw std::invalid_argument("quantity must be positive");
        }

        // 2. Reject duplicate active order IDs.
        if (orders_.contains(id)) {
            return std::nullopt;
        }

        // 3. Matching logic is not implemented yet.
        throw std::logic_error("matching not implemented");
    }

    bool cancel_order(OrderId id) {
        auto found = orders_.find(id);
        if (found == orders_.end()) {
            return false;
        }

        const OrderLocation location = found->second;

        if (location.side == Side::Buy) {
            auto level_it = bids_.find(location.price);
            if (level_it == bids_.end()) {
                return false;
            }

            level_it->second.erase(location.iterator);

            if (level_it->second.empty()) {
                bids_.erase(level_it);
            }
        } else {
            auto level_it = asks_.find(location.price);
            if (level_it == asks_.end()) {
                return false;
            }

            level_it->second.erase(location.iterator);

            if (level_it->second.empty()) {
                asks_.erase(level_it);
            }
        }

        orders_.erase(found);
        return true;
    }

    std::optional<Price> best_bid() const {
        if (bids_.empty()) {
            return std::nullopt;
        }
        return bids_.begin()->first;
    }

    std::optional<Price> best_ask() const {
        if (asks_.empty()) {
            return std::nullopt;
        }
        return asks_.begin()->first;
    }

    std::vector<OrderId> order_ids_at_price(Side side, Price price) const {
        std::vector<OrderId> ids;

        if (side == Side::Buy) {
            auto level_it = bids_.find(price);
            if (level_it == bids_.end()) {
                return ids;
            }

            for (const auto& order : level_it->second) {
                ids.push_back(order.id);
            }
        } else {
            auto level_it = asks_.find(price);
            if (level_it == asks_.end()) {
                return ids;
            }

            for (const auto& order : level_it->second) {
                ids.push_back(order.id);
            }
        }

        return ids;
    }

    std::size_t order_count() const {
        return orders_.size();
    }

    std::optional<Quantity> order_quantity(OrderId id) const {
        auto found = orders_.find(id);

        if (found == orders_.end()) {
            return std::nullopt;
        }

        return found->second.iterator->quantity;
    }

    std::size_t bid_level_count() const {
        return bids_.size();
    }

    std::size_t ask_level_count() const {
        return asks_.size();
    }

private:
    BidBook bids_;
    AskBook asks_;
    std::unordered_map<OrderId, OrderLocation> orders_;
};

} // namespace ob