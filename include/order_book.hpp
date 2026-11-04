#pragma once 

#include <algorithm>
#include <cstdint>
#include <iterator>
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

    template <typename BookType>
    void match_against(
        BookType& opposite_book,
        OrderId incoming_id,
        Side incoming_side,
        Price limit_price,
        Quantity& remaining_quantity,
        MatchResult& result
    ) {
        while (remaining_quantity > 0 && !opposite_book.empty()) {
            
            // 1. Select the best opposite price level.
            auto level_it = opposite_book.begin();
            const Price resting_price = level_it->first;

            // 2. Check whether the prices cross.
            const bool is_crossing = incoming_side == Side::Buy ? limit_price >= resting_price : limit_price <= resting_price;

            if (!is_crossing) {
                break;
            }

            // 3. Select the oldest resting order.
            auto order_it = level_it->second.begin();

            // 4. Calculate execution quantity.
            const Quantity executed_quantity = std::min(remaining_quantity, order_it->quantity);

            // 5. Record the trade before mutating the book.
            result.trades.push_back(Trade{
                incoming_id,
                order_it->id,
                resting_price,
                executed_quantity
            });

            // 6. Update quantities.
            remaining_quantity -= executed_quantity;
            order_it->quantity -= executed_quantity;

            // 7. Remove fully filled resting orders.
            if (order_it->quantity == 0) {
                orders_.erase(order_it->id);
                level_it->second.erase(order_it);

                // 8. Remove empty price levels.
                if (level_it->second.empty()) {
                    opposite_book.erase(level_it);
                }
            }
        }
    }

    void rest_order(OrderId id, Side side, Price price, Quantity quantity) {
        // Preconditions enforced by submit_order():
        // quantity > 0
        // id is not active
        // side is valid

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
    }

public:
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

        if (side != Side::Buy && side != Side::Sell) {
            throw std::invalid_argument("invalid order side");
        }

        // 2. Reject duplicate active order IDs.
        if (orders_.contains(id)) {
            return std::nullopt;
        }

        // 3. Prepare matching result.
        MatchResult result{
            .trades = {},
            .resting_quantity = 0
        };

        // 4. Match against the opposite-side book.
        if (side == Side::Buy) {
            match_against(
                asks_,
                id,
                side,
                price,
                quantity,
                result
            );
        } else {
            match_against(
                bids_,
                id,
                side,
                price,
                quantity,
                result
            );
        }

        // 5. Rest any unmatched incoming quantity
        if (quantity > 0) {
            rest_order(id, side, price, quantity);
            result.resting_quantity = quantity;
        }

        return result;
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