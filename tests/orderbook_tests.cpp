#include "orderbook.hpp"

// Keep assertions active even when CMake is configured with Release/-DNDEBUG.
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

void CheckTrade(const Trade& t, OrderId bid, OrderId ask, Quantity quantity,
                Price execPrice, Price bidLimit, Price askLimit) {
    assert(t.GetBidTrade().orderId_ == bid);
    assert(t.GetAskTrade().orderId_ == ask);
    assert(t.GetQuantity() == quantity);
    assert(t.GetExecutionPrice() == execPrice);
    assert(t.GetBidTrade().price_ == bidLimit);
    assert(t.GetAskTrade().price_ == askLimit);
}

template <class Error, class Callback>
void ExpectThrows(Callback f) {
    bool threw = false;
    try { f(); }
    catch (const Error&) { threw = true; }
    assert(threw);
}

void TestBasicGtcAndBestPrice() {
    OrderBook b;
    assert(b.AddOrder(Order{OrderType::GoodTillCancel, 1, Side::Sell, 105, 2}).empty());
    assert(b.AddOrder(Order{OrderType::GoodTillCancel, 2, Side::Sell, 100, 3}).empty());
    auto t = b.AddOrder(Order{OrderType::GoodTillCancel, 3, Side::Buy, 105, 4});
    assert(t.size() == 2);
    CheckTrade(t[0], 3, 2, 3, 100, 105, 100);
    CheckTrade(t[1], 3, 1, 1, 105, 105, 105);
    assert(b.Size() == 1);
    assert(b.GetOrder(1)->GetRemainingQuantity() == 1);
    auto levels = b.GetOrderInfos();
    assert(levels.GetBids().empty());
    assert((levels.GetAsks() == LevelInfos{{105, 1}}));
}

void TestSellHitsBestBid() {
    OrderBook b;
    b.AddOrder(Order{OrderType::GoodTillCancel, 1, Side::Buy, 110, 2});
    b.AddOrder(Order{OrderType::GoodTillCancel, 2, Side::Buy, 105, 4});
    auto t = b.AddOrder(Order{OrderType::GoodTillCancel, 3, Side::Sell, 104, 3});
    assert(t.size() == 2);
    CheckTrade(t[0], 1, 3, 2, 110, 110, 104);
    CheckTrade(t[1], 2, 3, 1, 105, 105, 104);
    assert(b.GetOrder(2)->GetRemainingQuantity() == 3);
}

void TestFifoPartialAndCancel() {
    OrderBook b;
    b.AddOrder(Order{OrderType::GoodTillCancel, 10, Side::Sell, 100, 3});
    b.AddOrder(Order{OrderType::GoodTillCancel, 11, Side::Sell, 100, 5});
    b.AddOrder(Order{OrderType::GoodTillCancel, 12, Side::Sell, 100, 7});
    auto t = b.AddOrder(Order{OrderType::GoodTillCancel, 20, Side::Buy, 100, 6});
    assert(t.size() == 2);
    CheckTrade(t[0], 20, 10, 3, 100, 100, 100);
    CheckTrade(t[1], 20, 11, 3, 100, 100, 100);
    assert(b.GetOrder(11)->GetRemainingQuantity() == 2);
    assert(b.GetOrder(12)->GetRemainingQuantity() == 7);
    assert(b.CancelOrder(11));
    assert(!b.CancelOrder(11));
    auto t2 = b.AddOrder(Order{OrderType::GoodTillCancel, 21, Side::Buy, 100, 7});
    assert(t2.size() == 1);
    CheckTrade(t2[0], 21, 12, 7, 100, 100, 100);
    assert(b.Size() == 0);
}

void TestIoc() {
    OrderBook b;
    assert(b.AddOrder(Order{OrderType::FillAndKill, 50, Side::Buy, 100, 3}).empty());
    assert(b.Size() == 0);
    b.AddOrder(Order{OrderType::GoodTillCancel, 51, Side::Sell, 100, 2});
    auto t = b.AddOrder(Order{OrderType::FillAndKill, 52, Side::Buy, 101, 10});
    assert(t.size() == 1 && t[0].GetQuantity() == 2);
    assert(b.Size() == 0);
    assert(!b.HasOrder(52));
    // A completed IOC order ID is not a live ID.
    b.AddOrder(Order{OrderType::GoodTillCancel, 52, Side::Buy, 99, 1});
    assert(b.HasOrder(52));
}

void TestModifyPriorityAndValidation() {
    OrderBook b;
    b.AddOrder(Order{OrderType::GoodTillCancel, 1, Side::Buy, 100, 5});
    b.AddOrder(Order{OrderType::GoodTillCancel, 2, Side::Buy, 100, 5});
    assert(b.ModifyOrder(OrderModify{1, Side::Buy, 100, 9}).empty());
    auto t = b.AddOrder(Order{OrderType::FillAndKill, 3, Side::Sell, 100, 7});
    assert(t.size() == 2);
    assert(t[0].GetBidTrade().orderId_ == 2); // Modified ID=1 moved to the back.
    assert(t[1].GetBidTrade().orderId_ == 1);
    assert(b.GetOrder(1)->GetRemainingQuantity() == 7);
    assert(b.GetOrder(1)->GetInitialQuantity() == 9);

    ExpectThrows<std::invalid_argument>([&]{
        b.ModifyOrder(OrderModify{1, Side::Sell, 100, 4});
    });
    ExpectThrows<std::invalid_argument>([&]{
        b.ModifyOrder(OrderModify{1, Side::Buy, -1, 4});
    });
    ExpectThrows<std::invalid_argument>([&]{
        b.ModifyOrder(OrderModify{1, Side::Buy, 100, 0});
    });
    ExpectThrows<std::out_of_range>([&]{
        b.ModifyOrder(OrderModify{999, Side::Buy, 100, 3});
    });
    assert(b.GetOrder(1)->GetRemainingQuantity() == 7);
    assert(b.Size() == 1);

    b.AddOrder(Order{OrderType::GoodTillCancel, 4, Side::Sell, 101, 3});
    auto replacementTrades = b.ModifyOrder(OrderModify{1, Side::Buy, 101, 7});
    assert(replacementTrades.size() == 1);
    CheckTrade(replacementTrades[0], 1, 4, 3, 101, 101, 101);
    assert(b.GetOrder(1)->GetRemainingQuantity() == 4);
}

void TestSnapshotAndExternalIsolation() {
    OrderBook b;
    auto external = std::make_shared<Order>(OrderType::GoodTillCancel, 1, Side::Buy, 100, 3);
    b.AddOrder(external);
    *external = Order{OrderType::GoodTillCancel, 777, Side::Sell, 200, 90};
    assert(b.HasOrder(1) && !b.HasOrder(777));
    assert(b.GetOrder(1)->GetPrice() == 100);
    auto copied = b.GetOrder(1).value();
    copied = Order{OrderType::FillAndKill, 88, Side::Sell, 70, 8};
    assert(b.GetOrder(1)->GetPrice() == 100);

    // The snapshot remains readable after the book changes and dies.
    auto saved = b.GetOrderInfos();
    assert(b.CancelOrder(1));
    assert((saved.GetBids() == LevelInfos{{100, 3}}));
    assert(!b.GetOrder(1).has_value());
}

void TestWideAggregationAndValidation() {
    OrderBook b;
    const Quantity maxQ = std::numeric_limits<Quantity>::max();
    b.AddOrder(Order{OrderType::GoodTillCancel, 1, Side::Buy, 100, maxQ});
    b.AddOrder(Order{OrderType::GoodTillCancel, 2, Side::Buy, 100, maxQ});
    assert(b.GetOrderInfos().GetBids().front().quantity_ ==
           static_cast<LevelQuantity>(maxQ) * 2);
    ExpectThrows<std::invalid_argument>([&]{
        b.AddOrder(Order{OrderType::GoodTillCancel, 1, Side::Sell, 100, 1});
    });
    ExpectThrows<std::invalid_argument>([&]{
        b.AddOrder(OrderPointer{});
    });
    ExpectThrows<std::invalid_argument>([]{
        Order o{OrderType::GoodTillCancel, 1, Side::Buy, 0, 1}; (void)o;
    });
    ExpectThrows<std::invalid_argument>([]{
        Order o{OrderType::GoodTillCancel, 1, Side::Sell, 100, 0}; (void)o;
    });
    ExpectThrows<std::invalid_argument>([]{
        Order o{static_cast<OrderType>(99), 1, Side::Sell, 100, 1}; (void)o;
    });
    ExpectThrows<std::invalid_argument>([]{
        Order o{OrderType::FillAndKill, 1, static_cast<Side>(99), 100, 1}; (void)o;
    });
    assert(b.Size() == 2);
}

// Independent, deliberately simple O(n) matching oracle for differential tests.
// It tracks resting orders in a vector and finds the best crossing order by scan.
struct OracleOrder {
    OrderId id;
    Side side;
    Price price;
    Quantity initial;
    Quantity remaining;
    std::uint64_t insertionSequence;
};
struct OracleTrade { OrderId bid, ask; Price bidLimit, askLimit, exec; Quantity quantity; };

class Oracle {
public:
    std::vector<OracleOrder> live;
    std::uint64_t sequence = 0;

    std::vector<OracleTrade> Add(OrderId id, Side side, Price price,
                                 Quantity quantity, OrderType type) {
        std::vector<OracleTrade> results;
        Quantity remaining = quantity;
        while (remaining > 0) {
            auto chosen = live.end();
            for (auto it = live.begin(); it != live.end(); ++it) {
                if (it->side == side) continue;
                if (side == Side::Buy && it->price > price) continue;
                if (side == Side::Sell && it->price < price) continue;
                if (chosen == live.end() ||
                    (side == Side::Buy && it->price < chosen->price) ||
                    (side == Side::Sell && it->price > chosen->price) ||
                    (it->price == chosen->price && it->insertionSequence < chosen->insertionSequence))
                    chosen = it;
            }
            if (chosen == live.end()) break;
            const auto q = std::min(remaining, chosen->remaining);
            if (side == Side::Buy)
                results.push_back({id, chosen->id, price, chosen->price, chosen->price, q});
            else
                results.push_back({chosen->id, id, chosen->price, price, chosen->price, q});
            remaining -= q;
            chosen->remaining -= q;
            if (chosen->remaining == 0) live.erase(chosen);
        }
        if (remaining && type == OrderType::GoodTillCancel)
            live.push_back({id, side, price, quantity, remaining, sequence++});
        return results;
    }

    void Cancel(OrderId id) {
        const auto it = std::find_if(live.begin(), live.end(),
                                     [id](const auto& o){return o.id == id;});
        assert(it != live.end());
        live.erase(it);
    }
};

void Verify(const OrderBook& book, const Oracle& oracle) {
    assert(book.Size() == oracle.live.size());
    std::map<Price, LevelQuantity, std::greater<Price>> bids;
    std::map<Price, LevelQuantity> asks;
    for (const auto& o : oracle.live) {
        assert(book.HasOrder(o.id));
        const auto actual = book.GetOrder(o.id);
        assert(actual);
        assert(actual->GetSide() == o.side);
        assert(actual->GetPrice() == o.price);
        assert(actual->GetRemainingQuantity() == o.remaining);
        assert(actual->GetInitialQuantity() == o.initial);
        if (o.side == Side::Buy) bids[o.price] += o.remaining;
        else asks[o.price] += o.remaining;
    }
    const auto levels = book.GetOrderInfos();
    assert(levels.GetBids().size() == bids.size());
    assert(levels.GetAsks().size() == asks.size());
    std::size_t i = 0;
    for (const auto& [price, q] : bids) {
        assert((levels.GetBids()[i++] == LevelInfo{price, q}));
    }
    i = 0;
    for (const auto& [price, q] : asks) {
        assert((levels.GetAsks()[i++] == LevelInfo{price, q}));
    }
    if (!bids.empty() && !asks.empty()) assert(bids.begin()->first < asks.begin()->first);
}

void CompareTrades(const Trades& actual, const std::vector<OracleTrade>& expected) {
    assert(actual.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
        CheckTrade(actual[i], expected[i].bid, expected[i].ask,
                   expected[i].quantity, expected[i].exec,
                   expected[i].bidLimit, expected[i].askLimit);
}

void TestRandomizedAgainstOracle() {
    OrderBook book;
    Oracle oracle;
    std::mt19937_64 rng(0xC0FFEE1234ULL);
    OrderId nextId = 1;
    for (int step = 0; step < 12000; ++step) {
        const auto operation = rng() % 100;
        if (oracle.live.empty() || operation < 62) {
            const OrderId id = nextId++;
            const Side side = rng() % 2 ? Side::Buy : Side::Sell;
            const Price price = static_cast<Price>(90 + rng() % 22);
            const Quantity q = static_cast<Quantity>(1 + rng() % 20);
            const OrderType type = rng() % 4 ? OrderType::GoodTillCancel : OrderType::FillAndKill;
            CompareTrades(book.AddOrder(Order{type, id, side, price, q}),
                          oracle.Add(id, side, price, q, type));
        } else if (operation < 81) {
            const auto selected = oracle.live.at(rng() % oracle.live.size());
            assert(book.CancelOrder(selected.id));
            oracle.Cancel(selected.id);
        } else {
            const auto selected = oracle.live.at(rng() % oracle.live.size());
            const Price newPrice = static_cast<Price>(90 + rng() % 22);
            const Quantity newQ = static_cast<Quantity>(1 + rng() % 20);
            auto actual = book.ModifyOrder(OrderModify{selected.id, selected.side, newPrice, newQ});
            oracle.Cancel(selected.id);
            CompareTrades(actual, oracle.Add(selected.id, selected.side, newPrice, newQ,
                                             OrderType::GoodTillCancel));
        }
        Verify(book, oracle);
    }
}

} // namespace

int main() {
    TestBasicGtcAndBestPrice();
    TestSellHitsBestBid();
    TestFifoPartialAndCancel();
    TestIoc();
    TestModifyPriorityAndValidation();
    TestSnapshotAndExternalIsolation();
    TestWideAggregationAndValidation();
    TestRandomizedAgainstOracle();
    std::cout << "All orderbook tests passed (including 12,000 randomized operations).\n";
}
