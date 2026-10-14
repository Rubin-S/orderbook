#include "replay.hpp"
#include <array>
#include <charconv>
#include <istream>
#include <ostream>

namespace orderbook_tools {
namespace {
std::string_view Trim(std::string_view field) {
    constexpr std::string_view whitespace = " \t\r\n\f\v";
    const auto begin = field.find_first_not_of(whitespace);
    if (begin == std::string_view::npos)
        return {};
    return field.substr(begin, field.find_last_not_of(whitespace) - begin + 1);
}
template <class T> T Number(std::string_view field) {
    if (field.empty() || field.find_first_not_of("0123456789") != std::string_view::npos)
        throw std::invalid_argument("Expected an unsigned decimal integer");
    T value{};
    const auto [end, error] = std::from_chars(field.data(), field.data() + field.size(), value);
    if (error != std::errc{} || end != field.data() + field.size())
        throw std::invalid_argument("Integer outside supported range");
    return value;
}
Side ParseSide(std::string_view value) {
    if (value == "BUY")
        return Side::Buy;
    if (value == "SELL")
        return Side::Sell;
    throw std::invalid_argument("Side must be BUY or SELL");
}
}

Command ParseCommand(std::string_view line) {
    std::array<std::string_view, 6> fields{};
    std::size_t count = 0;
    while (true) {
        if (count == fields.size())
            throw std::invalid_argument("Too many fields");
        const auto comma = line.find(',');
        fields[count++] = Trim(line.substr(0, comma));
        if (comma == std::string_view::npos)
            break;
        line.remove_prefix(comma + 1);
    }
    Command command;
    if (fields[0] == "CANCEL" && count == 2) {
        command.action = Action::Cancel;
        command.id = Number<OrderId>(fields[1]);
        return command;
    }
    if (fields[0] == "ADD" && count == 6)
        command.action = Action::Add;
    else if (fields[0] == "MODIFY" && count == 5)
        command.action = Action::Modify;
    else
        throw std::invalid_argument("Unknown command or incorrect field count");
    command.id = Number<OrderId>(fields[1]);
    command.side = ParseSide(fields[2]);
    command.price = Number<Price>(fields[3]);
    command.quantity = Number<Quantity>(fields[4]);
    if (command.action == Action::Add) {
        if (fields[5] == "GTC")
            command.type = OrderType::GoodTillCancel;
        else if (fields[5] == "IOC")
            command.type = OrderType::FillAndKill;
        else
            throw std::invalid_argument("Type must be GTC or IOC");
    }

    (void)Order(command.type, command.id, command.side, command.price, command.quantity);
    return command;
}

CommandResult ExecuteCommand(OrderBook& book, const Command& command) {
    switch (command.action) {
    case Action::Add:
        return {book.AddOrder(
                    Order(command.type, command.id, command.side, command.price, command.quantity)),
                std::nullopt};
    case Action::Modify:
        return {book.ModifyOrder(
                    OrderModify(command.id, command.side, command.price, command.quantity)),
                std::nullopt};
    case Action::Cancel:
        return {{}, book.CancelOrder(command.id)};
    }
    throw std::invalid_argument("Invalid command action");
}

ReplaySummary Replay(std::istream& input, std::ostream& output) {
    OrderBook book;
    ReplaySummary summary;
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        const auto content = Trim(line);
        if (content.empty() || content.front() == '#')
            continue;
        try {
            const auto command = ParseCommand(content);
            const auto result = ExecuteCommand(book, command);
            ++summary.commands;
            for (const auto& trade : result.trades) {
                ++summary.trades;
                summary.executedQuantity += trade.GetQuantity();
                output << "TRADE," << trade.GetBidTrade().orderId_ << ','
                       << trade.GetAskTrade().orderId_ << ',' << trade.GetExecutionPrice() << ','
                       << trade.GetQuantity() << '\n';
            }
            if (result.cancelled.has_value())
                output << "CANCEL," << command.id << ','
                       << (*result.cancelled ? "CANCELLED" : "NOT_FOUND") << '\n';
            if (!output)
                throw std::runtime_error("Output write failed");
        } catch (const std::exception& e) {
            throw ReplayError(lineNumber, e.what());
        }
    }
    if (input.bad() || !input.eof())
        throw ReplayError(lineNumber + 1, "Input read failed");
    summary.restingOrders = book.Size();
    const auto depth = book.GetOrderInfos();
    output << "SUMMARY," << summary.commands << ',' << summary.trades << ','
           << summary.restingOrders << '\n';
    for (const auto& level : depth.GetBids())
        output << "BID," << level.price_ << ',' << level.quantity_ << '\n';
    for (const auto& level : depth.GetAsks())
        output << "ASK," << level.price_ << ',' << level.quantity_ << '\n';
    if (!output)
        throw ReplayError(lineNumber, "Output write failed");
    return summary;
}
}
