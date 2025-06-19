#ifdef WIN32
#include <sdkddkver.h>
#endif
#include "seabattle.h"
#include <atomic>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <string_view>

namespace net = boost::asio;
using net::ip::tcp;
using namespace std::literals;

// Function to print the pair of fields
void PrintFieldPair(const SeabattleField& left, const SeabattleField& right) {
    auto left_pad = "  "s;
    auto delimeter = "    "s;
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
    for (size_t i = 0; i < SeabattleField::field_size; ++i) {
        std::cout << left_pad;
        left.PrintLine(std::cout, i);
        std::cout << delimeter;
        right.PrintLine(std::cout, i);
        std::cout << std::endl;
    }
    std::cout << left_pad;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << delimeter;
    SeabattleField::PrintDigitLine(std::cout);
    std::cout << std::endl;
}

// Read exactly `sz` bytes from the socket
template <size_t sz>
static std::optional<std::string> ReadExact(tcp::socket& socket) {
    boost::array<char, sz> buf;
    boost::system::error_code ec;
    net::read(socket, net::buffer(buf), net::transfer_exactly(sz), ec);
    if (ec) {
        return std::nullopt;
    }
    return {{buf.data(), sz}};
}

// Write exactly `data.size()` bytes to the socket
static bool WriteExact(tcp::socket& socket, std::string_view data) {
    boost::system::error_code ec;
    net::write(socket, net::buffer(data), net::transfer_exactly(data.size()), ec);
    return !ec;
}

// SeabattleAgent class implementation
class SeabattleAgent {
public:
    SeabattleAgent(const SeabattleField& field)
        : my_field_(field) {}

    void StartGame(tcp::socket& socket, bool my_initiative) {
        // Display initial fields
        PrintFields();

        while (!IsGameEnded()) {
            if (my_initiative) {
                // My turn to make a move
                std::pair<int, int> move = GetNextMove();
                SendMove(socket, move);
                auto result = ReadResult(socket);
                if (!result) {
                    std::cerr << "Failed to read result from opponent." << std::endl;
                    return;
                }
                ProcessResult(move, *result);
                PrintFields();
                my_initiative = (*result != SeabattleField::ShotResult::MISS);
            } else {
                // Opponent's turn
                std::cout << "Waiting for your turn..." << std::endl;

                auto move_opt = ReadMove(socket);
                if (!move_opt) {
                    std::cerr << "Failed to read move from opponent." << std::endl;
                    return;
                }

                auto [x, y] = *move_opt;
                std::cout << "Shot to " << MoveToString({x, y}) << std::endl;

                auto result = my_field_.Shoot(x, y);
                SendResult(socket, result);

                PrintFields();
                my_initiative = (result == SeabattleField::ShotResult::MISS);
            }
        }
        std::cout << "Game over!" << std::endl;
    }

    
private:
    static std::optional<std::pair<int, int>> ParseMove(const std::string_view& sv) {
        if (sv.size() != 2) return std::nullopt;
        int p1 = sv[0] - 'A', p2 = sv[1] - '1';
        if (p1 < 0 || p1 >= SeabattleField::field_size || p2 < 0 || p2 >= SeabattleField::field_size) {
            return std::nullopt;
        }
        return {{p2, p1}};
    }

    static std::string MoveToString(std::pair<int, int> move) {
        char buff[] = {static_cast<char>(move.second) + 'A', static_cast<char>(move.first) + '1'};
        return {buff, 2};
    }

    void PrintFields() const {
        PrintFieldPair(my_field_, other_field_);
    }

    bool IsGameEnded() const {
        return my_field_.IsLoser() || other_field_.IsLoser();
    }

    std::pair<int, int> GetNextMove() {
        while (true) {
            std::cout << "Your turn: ";
            std::string input;
            std::cin >> input;
            auto move_opt = ParseMove(input);
            if (move_opt && other_field_((*move_opt).first, (*move_opt).second) == SeabattleField::State::UNKNOWN) {
                return *move_opt;
            }
            std::cout << "Invalid move. Try again." << std::endl;
        }
    }

    void SendMove(tcp::socket& socket, std::pair<int, int> move) {
        std::string move_str = MoveToString(move);
        if (!WriteExact(socket, move_str)) {
            throw std::runtime_error("Failed to send move.");
        }
    }

    std::optional<std::pair<int, int>> ReadMove(tcp::socket& socket) {
        auto move_str_opt = ReadExact<2>(socket);
        if (!move_str_opt) {
            return std::nullopt;
        }
        return ParseMove(*move_str_opt);
    }

    void SendResult(tcp::socket& socket, SeabattleField::ShotResult result) {
        char res_char = static_cast<char>(result);
        if (!WriteExact(socket, std::string(1, res_char))) {
            throw std::runtime_error("Failed to send result.");
        }
    }

    std::optional<SeabattleField::ShotResult> ReadResult(tcp::socket& socket) {
        auto res_str_opt = ReadExact<1>(socket);
        if (!res_str_opt) {
            return std::nullopt;
        }
        return static_cast<SeabattleField::ShotResult>(res_str_opt->at(0));
    }

    void ProcessResult(std::pair<int, int> move, SeabattleField::ShotResult result) {
        size_t x = move.first, y = move.second;
        switch (result) {
            case SeabattleField::ShotResult::MISS:
                other_field_.MarkMiss(x, y);
                std::cout << "Miss!" << std::endl;
                break;
            case SeabattleField::ShotResult::HIT:
                other_field_.MarkHit(x, y);
                std::cout << "Hit!" << std::endl;
                break;
            case SeabattleField::ShotResult::KILL:
                other_field_.MarkKill(x, y);
                std::cout << "Kill!" << std::endl;
                break;
        }
    }

private:
    SeabattleField my_field_;
    SeabattleField other_field_{SeabattleField::State::UNKNOWN};
};

// Server initialization
void StartServer(const SeabattleField& field, unsigned short port) {
    try {
        net::io_context io_context;
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
        std::cout << "Waiting for connection... " << std::endl;

        tcp::socket socket(io_context);
        acceptor.accept(socket);

        SeabattleAgent agent(field);
        agent.StartGame(socket, false);
    } catch (std::exception& e) {
        std::cerr << "Exception in server: " << e.what() << std::endl;
    }
}

// Client initialization
void StartClient(const SeabattleField& field, const std::string& ip_str, unsigned short port) {
    try {
        net::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::socket socket(io_context);
        auto endpoints = resolver.resolve(ip_str, std::to_string(port));
        net::connect(socket, endpoints);

        SeabattleAgent agent(field);
        agent.StartGame(socket, true);
    } catch (std::exception& e) {
        std::cerr << "Exception in client: " << e.what() << std::endl;
    }
}

int main(int argc, const char** argv) {
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: program <seed> [<ip>] <port>" << std::endl;
        return 1;
    }

    std::mt19937 engine(std::stoi(argv[1]));
    SeabattleField fieldL = SeabattleField::GetRandomField(engine);

    if (argc == 3) {
        StartServer(fieldL, std::stoi(argv[2]));
    } else if (argc == 4) {
        StartClient(fieldL, argv[2], std::stoi(argv[3]));
    }

    return 0;
}
