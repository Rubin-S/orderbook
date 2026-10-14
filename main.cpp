#include "orderbook.hpp"

#include <iostream>

int main() {
    OrderBook book;

    book.AddOrder(Order{OrderType::GoodTillCancel, 101, Side::Sell, 10000, 10});
    book.AddOrder(Order{OrderType::GoodTillCancel, 102, Side::Sell, 10010, 5});

    const Trades trades = book.AddOrder(Order{OrderType::FillAndKill, 103, Side::Buy, 10020, 20});

    for (const Trade& trade : trades) {
        std::cout << "BUY " << trade.GetBidTrade().orderId_ << " / SELL "
                  << trade.GetAskTrade().orderId_ << " / qty " << trade.GetQuantity()
                  << " / execution price (ticks) " << trade.GetExecutionPrice() << '\n';
    }

    std::cout << "Resting orders: " << book.Size() << '\n';
    const auto snapshot = book.GetOrderInfos();
    std::cout << "Bid levels: " << snapshot.GetBids().size()
              << ", Ask levels: " << snapshot.GetAsks().size() << '\n';
}
