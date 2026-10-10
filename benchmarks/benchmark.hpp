#pragma once
#include "tools/replay.hpp"
#include <span>

namespace orderbook_tools {
struct BenchmarkOptions {
    std::size_t operations{100000};
    std::size_t repetitions{3};
    std::uint64_t seed{42};
};
enum class Scenario { Resting, Matching, Modify, Mixed };
struct Workload {
    Scenario scenario;
    std::vector<Command> commands;
    std::size_t expectedTrades{};
    std::uint64_t expectedQuantity{};
    std::size_t expectedResting{};
    LevelInfos expectedBids;
    LevelInfos expectedAsks;
};
struct Measurement {
    std::size_t repetition{};
    std::size_t operations{};
    std::size_t trades{};
    std::uint64_t executedQuantity{};
    std::size_t restingOrders{};
    std::uint64_t elapsedNs{};
    double operationsPerSecond{};
    std::uint64_t p50{};
    std::uint64_t p99{};
    std::size_t latencySamples{};
};
struct LatencyPercentiles {
    std::uint64_t p50;
    std::uint64_t p99;
};
LatencyPercentiles CalculatePercentiles(std::vector<std::uint64_t> samples);
BenchmarkOptions ParseBenchmarkOptions(std::span<const std::string_view> args);
Workload MakeWorkload(Scenario scenario, std::size_t count, std::uint64_t seed);
Measurement Measure(const Workload& workload, std::size_t repetition);
void RunBenchmarks(const BenchmarkOptions& options, std::ostream& output);
} // namespace orderbook_tools
