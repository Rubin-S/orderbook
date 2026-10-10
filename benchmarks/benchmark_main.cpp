#include "benchmark.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: orderbook_bench [--operations N] [--repetitions N] [--seed N]\n"
                     "Defaults: 100000 operations (1..1000000), 3 repetitions (1..30), seed 42 "
                     "(uint64).\n";
        return 0;
    }
    try {
        std::vector<std::string_view> args;
        for (int i = 1; i < argc; ++i)
            args.emplace_back(argv[i]);
        orderbook_tools::RunBenchmarks(orderbook_tools::ParseBenchmarkOptions(args), std::cout);
        std::cout.flush();
        if (!std::cout)
            throw std::runtime_error("Benchmark output write failed");
    } catch (const std::exception& e) {
        std::cerr << "orderbook_bench: " << e.what() << '\n';
        return 2;
    }
}
