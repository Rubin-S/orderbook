#include "benchmarks/benchmark.hpp"
#include <cmath>
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
BenchmarkOptions Options(std::initializer_list<std::string_view> args) {
    return ParseBenchmarkOptions(std::span(args.begin(), args.size()));
}
void Arguments() {
    auto options = Options({});
    CHECK(options.operations == 100000 && options.repetitions == 3 && options.seed == 42);
    options =
        Options({"--operations", "1", "--repetitions", "30", "--seed", "18446744073709551615"});
    CHECK(options.operations == 1 && options.repetitions == 30 &&
          options.seed == std::numeric_limits<std::uint64_t>::max());
    CHECK(Options({"--operations", "1000000", "--seed", "0"}).operations == 1000000);
    for (auto name : {"--operations", "--repetitions", "--seed"}) {
        Reject([&] { Options({name}); });
        Reject([&] { Options({name, "1", name, "2"}); });
        for (auto value : {"", "-1", "+1", "12x", " 1", "18446744073709551616"})
            Reject([&] { Options({name, value}); });
    }
    for (auto value : {"0", "1000001"})
        Reject([&] { Options({"--operations", value}); });
    for (auto value : {"0", "31"})
        Reject([&] { Options({"--repetitions", value}); });
    Reject([] { Options({"--unknown", "1"}); });
    Reject([] { MakeWorkload(Scenario::Mixed, 0, 42); });
    Reject([] { MakeWorkload(Scenario::Mixed, 1000001, 42); });
}
void Workloads() {
    for (auto scenario :
         {Scenario::Resting, Scenario::Matching, Scenario::Modify, Scenario::Mixed}) {
        const auto a = MakeWorkload(scenario, 65, 42);
        CHECK(a.commands == MakeWorkload(scenario, 65, 42).commands);
        CHECK(a.commands != MakeWorkload(scenario, 65, 43).commands);
        for (std::size_t count = 1; count <= 65; ++count) {
            const auto workload = MakeWorkload(scenario, count, 42);
            CHECK(workload.commands.size() == count);
            OrderBook book;
            std::size_t trades = 0;
            std::uint64_t quantity = 0;
            for (const auto& command : workload.commands) {
                const auto result = ExecuteCommand(book, command);
                if (command.action == Action::Cancel)
                    CHECK(result.cancelled == true);
                trades += result.trades.size();
                for (const auto& trade : result.trades)
                    quantity += trade.GetQuantity();
            }
            const auto depth = book.GetOrderInfos();
            CHECK(trades == workload.expectedTrades && quantity == workload.expectedQuantity);
            CHECK(book.Size() == workload.expectedResting);
            CHECK(depth.GetBids() == workload.expectedBids &&
                  depth.GetAsks() == workload.expectedAsks);

            if (scenario == Scenario::Resting) {
                const auto r = count % 64;
                const auto resting = r <= 32 ? r : 64 - r;
                CHECK(trades == 0 && quantity == 0 && book.Size() == resting);
                CHECK(depth.GetAsks().empty());
                CHECK(depth.GetBids().size() == (resting ? 1u : 0u));
                if (resting)
                    CHECK(depth.GetBids()[0].quantity_ == 3 * resting);
            } else if (scenario == Scenario::Matching) {
                CHECK(trades == count / 2 && quantity == 10 * (count / 2) &&
                      book.Size() == count % 2);
                CHECK(depth.GetBids().empty());
                if (count % 2)
                    CHECK(depth.GetAsks()[0].quantity_ == 10);
            } else if (scenario == Scenario::Modify) {
                const auto r = count % 3;
                CHECK(trades == 0 && quantity == 0 && book.Size() == (r ? 1u : 0u));
                CHECK(depth.GetAsks().empty());
                if (r)
                    CHECK(depth.GetBids()[0].quantity_ == (r == 1 ? 10u : 7u));
            } else {
                const auto r = count % 6;
                const auto sweeps = count / 6 + (r >= 4 ? 1 : 0);
                const auto resting = r == 1 || r == 4 ? 1u : (r == 2 || r == 3 ? 2u : 0u);
                CHECK(trades == 2 * sweeps && quantity == 12 * sweeps && book.Size() == resting);
                CHECK(depth.GetBids().empty());
                if (r == 1)
                    CHECK(depth.GetAsks()[0].quantity_ == 10);
                if (r == 2 || r == 3) {
                    CHECK(depth.GetAsks()[0].quantity_ == 10);
                    CHECK(depth.GetAsks()[1].quantity_ == (r == 2 ? 5u : 7u));
                }
                if (r == 4)
                    CHECK(depth.GetAsks()[0].quantity_ == 5);
            }
        }
    }
}
void Metrics() {
    const auto p = CalculatePercentiles({40, 10, 30, 20});
    CHECK(p.p50 == 20 && p.p99 == 40);
    const auto single = CalculatePercentiles({7});
    CHECK(single.p50 == 7 && single.p99 == 7);
    Reject([] { CalculatePercentiles({}); });
    for (auto scenario :
         {Scenario::Resting, Scenario::Matching, Scenario::Modify, Scenario::Mixed}) {
        for (std::size_t n : {1u, 6u, 65u}) {
            const auto workload = MakeWorkload(scenario, n, 42);
            const auto m = Measure(workload, 1);
            CHECK(m.operations == n && m.latencySamples == n && m.repetition == 1);
            CHECK(m.trades == workload.expectedTrades &&
                  m.executedQuantity == workload.expectedQuantity);
            CHECK(m.restingOrders == workload.expectedResting);
            CHECK(std::isfinite(m.operationsPerSecond) && m.operationsPerSecond >= 0);
            CHECK(m.p99 >= m.p50);
        }
    }
    auto corrupt = MakeWorkload(Scenario::Mixed, 6, 42);
    corrupt.expectedQuantity = 99;
    Reject([&] { Measure(corrupt, 1); });
    std::ostringstream output;
    RunBenchmarks(Options({"--operations", "6", "--repetitions", "1"}), output);
    const auto csv = output.str();
    CHECK(csv.find("# compiler=") != std::string::npos &&
          csv.find("# seed=42") != std::string::npos);
    CHECK(csv.find("scenario,repetition,operations,trades,executed_quantity,resting_orders,elapsed_"
                   "ns,operations_per_second,p50_ns,p99_ns,latency_samples\n") !=
          std::string::npos);
    CHECK(csv.find("mixed,1,6,2,12,0,") != std::string::npos);
    CHECK(csv.find("matching,1,6,3,30,0,") != std::string::npos);
    CHECK(csv.find("resting,1,6,0,0,6,") != std::string::npos);
    CHECK(csv.find("modify,1,6,0,0,0,") != std::string::npos);
}
int main() {
    try {
        Arguments();
        Workloads();
        Metrics();
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
