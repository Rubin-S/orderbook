#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

using Price = std::int32_t;
using Quantity = std::uint32_t;
using LevelQuantity = std::uint64_t;
using OrderId = std::uint64_t;

enum class OrderType { GoodTillCancel, FillAndKill };

enum class Side { Buy, Sell };

struct LevelInfo {
    Price price_;
    LevelQuantity quantity_;

    bool operator==(const LevelInfo&) const = default;
};

using LevelInfos = std::vector<LevelInfo>;

class OrderbookLevelInfos {
  public:
    OrderbookLevelInfos(LevelInfos bids, LevelInfos asks)
        : bids_(std::move(bids)), asks_(std::move(asks)) {}

    const LevelInfos& GetBids() const noexcept { return bids_; }
    const LevelInfos& GetAsks() const noexcept { return asks_; }

  private:
    LevelInfos bids_;
    LevelInfos asks_;
};

class Order {
  public:
    Order(OrderType type, OrderId id, Side side, Price price, Quantity quantity)
        : orderType_(type), orderId_(id), side_(side), price_(price), initialQuantity_(quantity),
          remainingQuantity_(quantity) {
        if (type != OrderType::GoodTillCancel && type != OrderType::FillAndKill)
            throw std::invalid_argument("Invalid order type");
        if (side != Side::Buy && side != Side::Sell)
            throw std::invalid_argument("Invalid order side");
        if (price <= 0)
            throw std::invalid_argument("Order price must be positive integer ticks");
        if (quantity == 0)
            throw std::invalid_argument("Order quantity must be positive");
    }

    OrderId GetOrderId() const noexcept { return orderId_; }
    Side GetSide() const noexcept { return side_; }
    Price GetPrice() const noexcept { return price_; }
    OrderType GetOrderType() const noexcept { return orderType_; }
    Quantity GetInitialQuantity() const noexcept { return initialQuantity_; }
    Quantity GetRemainingQuantity() const noexcept { return remainingQuantity_; }
    Quantity GetFilledQuantity() const noexcept { return initialQuantity_ - remainingQuantity_; }
    bool IsFilled() const noexcept { return remainingQuantity_ == 0; }

  private:
    friend class OrderBook;

    void Fill(Quantity quantity) {
        if (quantity == 0 || quantity > remainingQuantity_)
            throw std::logic_error("Fill must be positive and <= remaining quantity");
        remainingQuantity_ -= quantity;
    }

    OrderType orderType_;
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity initialQuantity_;
    Quantity remainingQuantity_;
};

using OrderPointer = std::shared_ptr<Order>;
using OrderPointers = std::list<OrderPointer>;

class OrderModify {
  public:
    OrderModify(OrderId id, Side side, Price price, Quantity quantity)
        : orderId_(id), side_(side), price_(price), quantity_(quantity) {}

    OrderId GetOrderId() const noexcept { return orderId_; }
    Side GetSide() const noexcept { return side_; }
    Price GetPrice() const noexcept { return price_; }
    Quantity GetQuantity() const noexcept { return quantity_; }

    OrderPointer ToOrderPointer(OrderType type) const {
        return std::make_shared<Order>(type, orderId_, side_, price_, quantity_);
    }

  private:
    OrderId orderId_;
    Side side_;
    Price price_;
    Quantity quantity_;
};

struct TradeInfo {
    OrderId orderId_;
    Price price_;
    Quantity quantity_;

    bool operator==(const TradeInfo&) const = default;
};

class Trade {
  public:
    Trade(TradeInfo bid, TradeInfo ask, Price executionPrice)
        : bidTrade_(bid), askTrade_(ask), executionPrice_(executionPrice) {
        if (bid.quantity_ == 0 || bid.quantity_ != ask.quantity_ || executionPrice <= 0)
            throw std::invalid_argument("Invalid trade");
    }

    const TradeInfo& GetBidTrade() const noexcept { return bidTrade_; }
    const TradeInfo& GetAskTrade() const noexcept { return askTrade_; }
    Price GetExecutionPrice() const noexcept { return executionPrice_; }
    Quantity GetQuantity() const noexcept { return bidTrade_.quantity_; }

  private:
    TradeInfo bidTrade_;
    TradeInfo askTrade_;
    Price executionPrice_;
};

using Trades = std::vector<Trade>;

class OrderBook {
  public:
    OrderBook() = default;

    OrderBook(const OrderBook&) = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) = delete;
    OrderBook& operator=(OrderBook&&) = delete;

    Trades AddOrder(const OrderPointer& submitted) {
        if (!submitted)
            throw std::invalid_argument("Order pointer cannot be null");
        if (orders_.contains(submitted->GetOrderId()))
            throw std::invalid_argument("Duplicate live order ID");
        if (submitted->GetRemainingQuantity() != submitted->GetInitialQuantity())
            throw std::invalid_argument("Cannot submit a previously filled order");

        auto incoming = std::make_shared<Order>(*submitted);
        Trades trades = incoming->GetSide() == Side::Buy ? MatchIncoming(incoming, asks_)
                                                         : MatchIncoming(incoming, bids_);

        if (!incoming->IsFilled() && incoming->GetOrderType() == OrderType::GoodTillCancel) {
            if (incoming->GetSide() == Side::Buy)
                RestOrder(incoming, bids_);
            else
                RestOrder(incoming, asks_);
        }
        return trades;
    }

    Trades AddOrder(const Order& order) { return AddOrder(std::make_shared<Order>(order)); }

    bool CancelOrder(OrderId id) {
        const auto found = orders_.find(id);
        if (found == orders_.end())
            return false;

        const OrderEntry entry = found->second;
        const auto price = entry.order_->GetPrice();
        if (entry.order_->GetSide() == Side::Buy)
            EraseFromLevel(bids_, price, entry.location_);
        else
            EraseFromLevel(asks_, price, entry.location_);
        orders_.erase(found);
        return true;
    }

    Trades ModifyOrder(const OrderModify& change) {
        const auto found = orders_.find(change.GetOrderId());
        if (found == orders_.end())
            throw std::out_of_range("Cannot modify a non-resting order");
        if (change.GetSide() != found->second.order_->GetSide())
            throw std::invalid_argument("Modification cannot change order side");

        auto replacement = change.ToOrderPointer(found->second.order_->GetOrderType());
        CancelOrder(change.GetOrderId());
        return AddOrder(replacement);
    }

    std::size_t Size() const noexcept { return orders_.size(); }
    bool HasOrder(OrderId id) const { return orders_.contains(id); }

    std::optional<Order> GetOrder(OrderId id) const {
        const auto found = orders_.find(id);
        if (found == orders_.end())
            return std::nullopt;
        return *found->second.order_;
    }

    OrderbookLevelInfos GetOrderInfos() const {
        LevelInfos bidInfos;
        LevelInfos askInfos;
        bidInfos.reserve(bids_.size());
        askInfos.reserve(asks_.size());

        for (const auto& [price, queue] : bids_)
            bidInfos.push_back(LevelInfo{price, SumLevel(queue)});
        for (const auto& [price, queue] : asks_)
            askInfos.push_back(LevelInfo{price, SumLevel(queue)});
        return {std::move(bidInfos), std::move(askInfos)};
    }

  private:
    struct OrderEntry {
        OrderPointer order_;
        OrderPointers::iterator location_;
    };

    std::map<Price, OrderPointers, std::greater<Price>> bids_;
    std::map<Price, OrderPointers, std::less<Price>> asks_;
    std::unordered_map<OrderId, OrderEntry> orders_;

    template <class OpposingLevels>
    Trades MatchIncoming(const OrderPointer& incoming, OpposingLevels& opposing) {
        Trades trades;
        while (!incoming->IsFilled() && !opposing.empty()) {
            auto best = opposing.begin();
            const Price restingPrice = best->first;
            const bool crosses = incoming->GetSide() == Side::Buy
                                     ? incoming->GetPrice() >= restingPrice
                                     : incoming->GetPrice() <= restingPrice;
            if (!crosses)
                break;

            const auto resting = best->second.front();
            const Quantity quantity =
                std::min(incoming->GetRemainingQuantity(), resting->GetRemainingQuantity());
            const auto& bid = incoming->GetSide() == Side::Buy ? incoming : resting;
            const auto& ask = incoming->GetSide() == Side::Sell ? incoming : resting;

            trades.emplace_back(TradeInfo{bid->GetOrderId(), bid->GetPrice(), quantity},
                                TradeInfo{ask->GetOrderId(), ask->GetPrice(), quantity},
                                restingPrice);

            incoming->Fill(quantity);
            resting->Fill(quantity);

            if (resting->IsFilled()) {
                orders_.erase(resting->GetOrderId());
                best->second.pop_front();
                if (best->second.empty())
                    opposing.erase(best);
            }
        }
        return trades;
    }

    template <class Levels> void RestOrder(const OrderPointer& order, Levels& levels) {
        const auto [level, created] = levels.try_emplace(order->GetPrice());
        try {
            level->second.push_back(order);
        } catch (...) {
            if (created)
                levels.erase(level);
            throw;
        }
        const auto location = std::prev(level->second.end());
        try {
            const auto [it, inserted] =
                orders_.emplace(order->GetOrderId(), OrderEntry{order, location});
            (void)it;
            if (!inserted)
                throw std::logic_error("Duplicate order index entry");
        } catch (...) {
            level->second.erase(location);
            if (level->second.empty())
                levels.erase(level);
            throw;
        }
    }

    template <class Levels>
    static void EraseFromLevel(Levels& levels, Price price, OrderPointers::iterator location) {
        const auto level = levels.find(price);
        if (level == levels.end())
            throw std::logic_error("Corrupt order index");
        level->second.erase(location);
        if (level->second.empty())
            levels.erase(level);
    }

    static LevelQuantity SumLevel(const OrderPointers& queue) {
        LevelQuantity result = 0;
        for (const auto& order : queue) {
            const LevelQuantity quantity = order->GetRemainingQuantity();
            if (result > std::numeric_limits<LevelQuantity>::max() - quantity)
                throw std::overflow_error("Aggregated level quantity overflow");
            result += quantity;
        }
        return result;
    }
};
