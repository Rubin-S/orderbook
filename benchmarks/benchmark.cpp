#include "benchmark.hpp"
#include <array>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <locale>
#include <ostream>
#include <random>

#ifndef ORDERBOOK_BUILD_TYPE
#define ORDERBOOK_BUILD_TYPE "unknown"
#endif

namespace orderbook_tools {
namespace {
constexpr std::size_t MaxOperations = 1000000;
void Validate(const BenchmarkOptions& options) {
    if (options.operations == 0 || options.operations > MaxOperations)
        throw std::invalid_argument("Operations must be in [1, 1000000]");
    if (options.repetitions == 0 || options.repetitions > 30)
        throw std::invalid_argument("Repetitions must be in [1, 30]");
}
const char* Name(Scenario scenario) {
    switch (scenario) {
    case Scenario::Resting:
        return "resting";
    case Scenario::Matching:
        return "matching";
    case Scenario::Modify:
        return "modify";
    case Scenario::Mixed:
        return "mixed";
    }
    throw std::invalid_argument("Invalid scenario");
}
struct Counts {
    std::size_t trades{};
    std::uint64_t quantity{};
};
void Apply(OrderBook& book, const Command& command, Counts& counts) {
    const auto result = ExecuteCommand(book, command);
    if (result.cancelled.has_value() && !*result.cancelled)
        throw std::runtime_error("Workload cancelled a non-resting ID");
    counts.trades += result.trades.size();
    for (const auto& trade : result.trades)
        counts.quantity += trade.GetQuantity();
}
void Verify(const Workload& workload, const OrderBook& book, const Counts& counts) {
    const auto depth = book.GetOrderInfos();
    if (counts.trades != workload.expectedTrades || counts.quantity != workload.expectedQuantity ||
        book.Size() != workload.expectedResting || depth.GetBids() != workload.expectedBids ||
        depth.GetAsks() != workload.expectedAsks)
        throw std::runtime_error("Benchmark result differs from expected state");
}
std::uint64_t Nanoseconds(std::chrono::steady_clock::duration duration) {
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    return value > 0 ? static_cast<std::uint64_t>(value) : 0;
}
}

BenchmarkOptions ParseBenchmarkOptions(std::span<const std::string_view> args) {
    BenchmarkOptions options;
    std::array<bool, 3> seen{};
    for (std::size_t i = 0; i < args.size(); i += 2) {
        const auto name = args[i];
        const auto slot = name == "--operations"    ? 0
                          : name == "--repetitions" ? 1
                          : name == "--seed"        ? 2
                                                    : -1;
        if (slot < 0)
            throw std::invalid_argument("Unknown option: " + std::string(name));
        if (seen[slot])
            throw std::invalid_argument("Duplicate option: " + std::string(name));
        seen[slot] = true;
        if (i + 1 == args.size())
            throw std::invalid_argument("Missing value for " + std::string(name));
        const auto field = args[i + 1];
        if (field.empty() || field.find_first_not_of("0123456789") != std::string_view::npos)
            throw std::invalid_argument("Expected unsigned decimal option value");
        std::uint64_t value{};
        const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
        if (error != std::errc{} || end != field.data() + field.size())
            throw std::invalid_argument("Option value outside supported range");
        if (slot == 0) {
            if (value == 0 || value > MaxOperations)
                throw std::invalid_argument("Operations must be in [1, 1000000]");
            options.operations = static_cast<std::size_t>(value);
        } else if (slot == 1) {
            if (value == 0 || value > 30)
                throw std::invalid_argument("Repetitions must be in [1, 30]");
            options.repetitions = static_cast<std::size_t>(value);
        } else
            options.seed = value;
    }
    Validate(options);
    return options;
}

Workload MakeWorkload(Scenario scenario, std::size_t count, std::uint64_t seed) {
    Validate({count, 1, seed});
    (void)Name(scenario);
    Workload workload{scenario, {}, 0, 0, 0, {}, {}};
    workload.commands.reserve(count);
    std::mt19937_64 random(seed);
    Price price{};
    auto add = [&](Command command) {
        if (workload.commands.size() < count)
            workload.commands.push_back(command);
    };
    for (OrderId cycle = 0; workload.commands.size() < count; ++cycle) {
        price = static_cast<Price>(10000 + random() % 1000);
        if (scenario == Scenario::Resting) {
            for (OrderId j = 1; j <= 32; ++j)
                add({Action::Add, cycle * 32 + j, Side::Buy, price, 3, OrderType::GoodTillCancel});
            for (OrderId j = 1; j <= 32; ++j)
                add({Action::Cancel, cycle * 32 + j});
        } else if (scenario == Scenario::Matching) {
            add({Action::Add, cycle * 2 + 1, Side::Sell, price, 10, OrderType::GoodTillCancel});
            add({Action::Add, cycle * 2 + 2, Side::Buy, price, 10, OrderType::FillAndKill});
        } else if (scenario == Scenario::Modify) {
            add({Action::Add, cycle + 1, Side::Buy, price, 10, OrderType::GoodTillCancel});
            add({Action::Modify, cycle + 1, Side::Buy, price + 1, 7});
            add({Action::Cancel, cycle + 1});
        } else {
            const auto id = cycle * 4;
            add({Action::Add, id + 1, Side::Sell, price, 10, OrderType::GoodTillCancel});
            add({Action::Add, id + 2, Side::Sell, price + 1, 5, OrderType::GoodTillCancel});
            add({Action::Modify, id + 2, Side::Sell, price + 1, 7});
            add({Action::Add, id + 3, Side::Buy, price + 1, 12, OrderType::FillAndKill});
            add({Action::Cancel, id + 2});
            add({Action::Add, id + 4, Side::Sell, price, 3, OrderType::FillAndKill});
        }
    }

    if (scenario == Scenario::Resting) {
        const auto r = count % 64;
        workload.expectedResting = r <= 32 ? r : 64 - r;
        if (workload.expectedResting)
            workload.expectedBids.push_back({price, 3 * workload.expectedResting});
    } else if (scenario == Scenario::Matching) {
        workload.expectedTrades = count / 2;
        workload.expectedQuantity = 10 * (count / 2);
        workload.expectedResting = count % 2;
        if (count % 2)
            workload.expectedAsks.push_back({price, 10});
    } else if (scenario == Scenario::Modify) {
        const auto r = count % 3;
        workload.expectedResting = r ? 1 : 0;
        if (r)
            workload.expectedBids.push_back({r == 1 ? price : price + 1, r == 1 ? 10u : 7u});
    } else {
        const auto r = count % 6;
        const auto sweeps = count / 6 + (r >= 4 ? 1 : 0);
        workload.expectedTrades = 2 * sweeps;
        workload.expectedQuantity = 12 * sweeps;
        if (r >= 1 && r <= 3)
            workload.expectedAsks.push_back({price, 10});
        if (r >= 2 && r <= 4)
            workload.expectedAsks.push_back({price + 1, r == 3 ? 7u : 5u});
        workload.expectedResting = workload.expectedAsks.size();
    }
    return workload;
}

LatencyPercentiles CalculatePercentiles(std::vector<std::uint64_t> samples) {
    if (samples.empty())
        throw std::invalid_argument("Percentiles need at least one sample");
    std::sort(samples.begin(), samples.end());

    const auto rank = [&](std::size_t p) {
        return (samples.size() / 100) * p + ((samples.size() % 100) * p + 99) / 100 - 1;
    };
    return {samples[rank(50)], samples[rank(99)]};
}

Measurement Measure(const Workload& workload, std::size_t repetition) {
    Validate({workload.commands.size(), 1, 0});
    {
        OrderBook warmup;
        Counts counts;
        for (const auto& command : workload.commands)
            Apply(warmup, command, counts);
        Verify(workload, warmup, counts);
    }
    Measurement measurement;
    measurement.repetition = repetition;
    measurement.operations = workload.commands.size();
    std::vector<std::uint64_t> samples(workload.commands.size());
    {
        OrderBook book;
        Counts counts;
        const auto begin = std::chrono::steady_clock::now();
        for (const auto& command : workload.commands)
            Apply(book, command, counts);
        const auto end = std::chrono::steady_clock::now();
        measurement.elapsedNs = Nanoseconds(end - begin);
        Verify(workload, book, counts);
        measurement.trades = counts.trades;
        measurement.executedQuantity = counts.quantity;
        measurement.restingOrders = book.Size();
    }
    {
        OrderBook book;
        Counts counts;
        for (std::size_t i = 0; i < workload.commands.size(); ++i) {
            const auto begin = std::chrono::steady_clock::now();
            Apply(book, workload.commands[i], counts);
            const auto end = std::chrono::steady_clock::now();
            samples[i] = Nanoseconds(end - begin);
        }
        Verify(workload, book, counts);
    }
    const auto percentiles = CalculatePercentiles(std::move(samples));
    measurement.p50 = percentiles.p50;
    measurement.p99 = percentiles.p99;
    measurement.latencySamples = workload.commands.size();
    measurement.operationsPerSecond = measurement.elapsedNs == 0
                                          ? 0.0
                                          : static_cast<double>(measurement.operations) * 1.0e9 /
                                                static_cast<double>(measurement.elapsedNs);
    return measurement;
}

void RunBenchmarks(const BenchmarkOptions& options, std::ostream& output) {
    Validate(options);

    output.imbue(std::locale::classic());
#if defined(__clang__)
    output << "# compiler=Clang " << __clang_version__ << '\n';
#elif defined(__GNUC__)
    output << "# compiler=GCC " << __VERSION__ << '\n';
#elif defined(_MSC_VER)
    output << "# compiler=MSVC " << _MSC_VER << '\n';
#else
    output << "# compiler=unknown\n";
#endif
    output << "# build=" << ORDERBOOK_BUILD_TYPE << "\n# seed=" << options.seed << '\n'
           << "scenario,repetition,operations,trades,executed_quantity,resting_orders,elapsed_ns,"
              "operations_per_second,p50_ns,p99_ns,latency_samples\n";
    output << std::fixed << std::setprecision(2);
    for (const auto scenario :
         {Scenario::Resting, Scenario::Matching, Scenario::Modify, Scenario::Mixed}) {
        const auto workload = MakeWorkload(scenario, options.operations, options.seed);
        for (std::size_t repetition = 1; repetition <= options.repetitions; ++repetition) {
            const auto m = Measure(workload, repetition);
            output << Name(scenario) << ',' << m.repetition << ',' << m.operations << ','
                   << m.trades << ',' << m.executedQuantity << ',' << m.restingOrders << ','
                   << m.elapsedNs << ',' << m.operationsPerSecond << ',' << m.p50 << ',' << m.p99
                   << ',' << m.latencySamples << '\n';
            if (!output)
                throw std::runtime_error("Benchmark output write failed");
        }
    }
}
}
