#pragma once
#include "orderbook.hpp"
#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>

namespace orderbook_tools {
enum class Action { Add, Modify, Cancel };
struct Command {
    Action action{Action::Cancel};
    OrderId id{};
    Side side{Side::Buy};
    Price price{};
    Quantity quantity{};
    OrderType type{OrderType::GoodTillCancel};
    bool operator==(const Command&) const = default;
};
struct CommandResult {
    Trades trades;
    std::optional<bool> cancelled;
};
struct ReplaySummary {
    std::size_t commands{};
    std::size_t trades{};
    std::uint64_t executedQuantity{};
    std::size_t restingOrders{};
};
class ReplayError : public std::runtime_error {
  public:
    ReplayError(std::size_t line, const std::string& message)
        : std::runtime_error("line " + std::to_string(line) + ": " + message), line_(line) {}
    std::size_t Line() const noexcept { return line_; }

  private:
    std::size_t line_;
};
Command ParseCommand(std::string_view line);
CommandResult ExecuteCommand(OrderBook& book, const Command& command);
ReplaySummary Replay(std::istream& input, std::ostream& output);
} // namespace orderbook_tools
