#include "orderbook.hpp"

#include <iostream>

int main() {
    OrderBook book;

    // Prices are integer ticks. Example: 10000 means $100.00 if tick=$0.01.
    book.AddOrder(Order{OrderType::GoodTillCancel, 101, Side::Sell, 10000, 10});
    book.AddOrder(Order{OrderType::GoodTillCancel, 102, Side::Sell, 10010, 5});

    // This crosses both ask price levels. The last 5 shares are discarded (IOC).
    const Trades trades = book.AddOrder(
        Order{OrderType::FillAndKill, 103, Side::Buy, 10020, 20});

    for (const Trade& trade : trades) {
        std::cout << "BUY " << trade.GetBidTrade().orderId_
                  << " / SELL " << trade.GetAskTrade().orderId_
                  << " / qty " << trade.GetQuantity()
                  << " / execution price (ticks) " << trade.GetExecutionPrice()
                  << '\n';
    }

    std::cout << "Resting orders: " << book.Size() << '\n';
    const auto snapshot = book.GetOrderInfos();
    std::cout << "Bid levels: " << snapshot.GetBids().size()
              << ", Ask levels: " << snapshot.GetAsks().size() << '\n';
}
