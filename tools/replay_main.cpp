#include "replay.hpp"
#include <fstream>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: orderbook_replay <file|->\nReplay ADD/MODIFY/CANCEL CSV commands; '-' "
                     "reads stdin.\n";
        return 0;
    }
    if (argc != 2 ||
        (std::string_view(argv[1]).starts_with('-') && std::string_view(argv[1]) != "-")) {
        std::cerr << "Usage: orderbook_replay <file|->\n";
        return 2;
    }
    try {
        if (std::string_view(argv[1]) == "-") {
            orderbook_tools::Replay(std::cin, std::cout);
        } else {
            std::ifstream input(argv[1]);
            if (!input)
                throw std::runtime_error("Cannot open input file: " + std::string(argv[1]));
            orderbook_tools::Replay(input, std::cout);
        }
        std::cout.flush();
        if (!std::cout)
            throw std::runtime_error("Output write failed");
    } catch (const std::exception& e) {
        std::cerr << "orderbook_replay: " << e.what() << '\n';
        return 2;
    }
}
