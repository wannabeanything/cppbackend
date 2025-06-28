#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

namespace {
    // Запускает функцию fn на n потоках, включая текущий
    template <typename Fn>
    void RunWorkers(unsigned n, const Fn& fn) {
        n = std::max(1u, n);
        std::vector<std::thread> workers;
        workers.reserve(n - 1);
        while (--n) {
            workers.emplace_back(fn);
        }
        fn();
        for (auto& worker : workers) {
            worker.join();
        }
    }
} // namespace

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // 1. Загружаем карту из файла и построить модель игры
        model::Game game = json_loader::LoadGame(argv[1]);

        // 2. Инициализируем io_context
        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        // 3. Обработчик сигналов
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, [[maybe_unused]] int signal_number) {
            if (!ec) {
                std::cout << "Signal caught, shutting down..."sv << std::endl;
                ioc.stop();
            }
        });

        // 4. HTTP обработчик
        http_handler::RequestHandler handler{game};

        // 5. Запуск HTTP-сервера
        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;
        http_server::ServeHttp(ioc, {address, port}, [&handler](auto&& req, auto&& send) {
            handler(std::forward<decltype(req)>(req), std::forward<decltype(send)>(send));
        });

        std::cout << "Server has started..."sv << std::endl;

        // 6. Запуск io_context
        RunWorkers(num_threads, [&ioc] { ioc.run(); });
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
