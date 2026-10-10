#include "tools/replay.hpp"
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

using namespace orderbook_tools;
#define CHECK(...)                                                                                 \
    do {                                                                                           \
        if (!(__VA_ARGS__))                                                                        \
            throw std::runtime_error("check failed at line " + std::to_string(__LINE__) +          \
                                     ": " #__VA_ARGS__);                                           \
    } while (false)

template <class F> void Reject(F action) {
    bool threw = false;
    try {
        action();
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}
std::string Run(std::string input) {
    std::istringstream in(input);
    std::ostringstream out;
    Replay(in, out);
    return out.str();
}
void GoldenAndFifo() {
    CHECK(Run("ADD,101,SELL,10000,10,GTC\nADD,102,SELL,10010,5,GTC\nADD,103,BUY,10020,12,IOC\n") ==
          "TRADE,103,101,10000,10\nTRADE,103,102,10010,2\nSUMMARY,3,2,1\nASK,10010,3\n");
    CHECK(Run("ADD,1,SELL,100,3,GTC\nADD,2,SELL,100,4,GTC\nADD,3,BUY,101,5,GTC\n") ==
          "TRADE,3,1,100,3\nTRADE,3,2,100,2\nSUMMARY,3,2,1\nASK,100,2\n");
    CHECK(Run("ADD,1,SELL,100,3,GTC\nADD,2,BUY,101,5,GTC\n") ==
          "TRADE,2,1,100,3\nSUMMARY,2,1,1\nBID,101,2\n");
    CHECK(Run("ADD,1,BUY,100,9,IOC\n") == "SUMMARY,1,0,0\n");
}
void CancellationModificationAndDepth() {
    CHECK(Run("CANCEL,9\nADD,1,BUY,90,3,GTC\nCANCEL,1\n") ==
          "CANCEL,9,NOT_FOUND\nCANCEL,1,CANCELLED\nSUMMARY,3,0,0\n");
    CHECK(Run("ADD,1,BUY,100,3,GTC\nADD,2,BUY,100,4,GTC\nMODIFY,1,BUY,100,2\nADD,3,SELL,100,5,"
              "IOC\n") == "TRADE,2,3,100,4\nTRADE,1,3,100,1\nSUMMARY,4,2,1\nBID,100,1\n");
    CHECK(Run("ADD,1,BUY,90,3,GTC\nADD,2,BUY,95,4,GTC\nADD,3,SELL,110,5,GTC\nADD,4,SELL,105,6,"
              "GTC\n") == "SUMMARY,4,0,4\nBID,95,4\nBID,90,3\nASK,105,6\nASK,110,5\n");
}
void WhitespaceAndBoundaries() {
    CHECK(Run("") == "SUMMARY,0,0,0\n");
    CHECK(Run(" \t\r\n # comment\r\n ADD , 0 , BUY , 2147483647 , 4294967295 , GTC "
              "\r\nCANCEL,0\r\n") == "CANCEL,0,CANCELLED\nSUMMARY,2,0,0\n");
    const auto max = ParseCommand("ADD,18446744073709551615,SELL,2147483647,4294967295,GTC");
    CHECK(max.id == std::numeric_limits<OrderId>::max());
    CHECK(max.quantity == std::numeric_limits<Quantity>::max());
    CHECK(max.price == std::numeric_limits<Price>::max());
    for (const auto* bad : {"",
                            "# comment",
                            "CANCEL",
                            "CANCEL,1,",
                            "CANCEL,-1",
                            "CANCEL,+1",
                            "CANCEL,18446744073709551616",
                            "CANCEL,12x",
                            "CANCEL,",
                            "cancel,1",
                            "ADD,1,BUY,1,1",
                            "ADD,1,BUY,1,1,GTC,extra",
                            "ADD,1,BID,1,1,GTC",
                            "ADD,1,BUY,1,1,FOK",
                            "ADD,1,BUY,2147483648,1,GTC",
                            "ADD,1,BUY,-1,1,GTC",
                            "ADD,1,BUY,+1,1,GTC",
                            "ADD,1,BUY,0,1,GTC",
                            "ADD,1,BUY,1,0,GTC",
                            "ADD,1,BUY,1,-1,GTC",
                            "ADD,1,BUY,1,+1,GTC",
                            "ADD,1,BUY,1,4294967296,GTC",
                            "MODIFY,1,BUY,1,1,extra",
                            "MODIFY,1,BUY,1",
                            "MODIFY,1,BUY,1,12x"})
        Reject([&] { (void)ParseCommand(bad); });
}
void ErrorsAndAccounting() {
    for (const auto* bad :
         {"bad", "ADD,1,BUY,100,2,GTC", "MODIFY,9,BUY,100,2", "MODIFY,1,SELL,100,2"}) {
        std::istringstream in(std::string("ADD,1,BUY,100,2,GTC\n") + bad + "\n");
        std::ostringstream out;
        bool rejected = false;
        try {
            Replay(in, out);
        } catch (const ReplayError& e) {
            rejected = true;
            CHECK(e.Line() == 2);
            CHECK(std::string(e.what()).find("line 2") != std::string::npos);
        }
        CHECK(rejected);
        CHECK(out.str().find("SUMMARY") == std::string::npos);
    }
    std::istringstream in("CANCEL,7\n\n# skip\ninvalid\n");
    std::ostringstream out;
    try {
        Replay(in, out);
        CHECK(false);
    } catch (const ReplayError& e) {
        CHECK(e.Line() == 4);
    }
    CHECK(out.str() == "CANCEL,7,NOT_FOUND\n");
    std::istringstream ok("ADD,1,SELL,100,3,GTC\nADD,2,BUY,100,2,IOC\n");
    std::ostringstream sink;
    const auto summary = Replay(ok, sink);
    CHECK(summary.commands == 2 && summary.trades == 1 && summary.executedQuantity == 2 &&
          summary.restingOrders == 1);
    std::istringstream broken;
    broken.setstate(std::ios::badbit);
    Reject([&] { Replay(broken, sink); });
}
int main() {
    try {
        GoldenAndFifo();
        CancellationModificationAndDepth();
        WhitespaceAndBoundaries();
        ErrorsAndAccounting();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
