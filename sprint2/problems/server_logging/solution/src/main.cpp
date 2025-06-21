#include "sdk.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/json.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>
#include "http_server.h"
#include "json_loader.h"
#include "request_handler.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;
namespace logging = boost::log;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(additional_data, "AdditionalData", json::value)

namespace {
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
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: game_server <game-config-json>"sv << std::endl;
        return EXIT_FAILURE;
    }

    try {
        http_handler::InitLogging();

        model::Game game = json_loader::LoadGame(argv[1]);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);
        
        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code& ec, int) {
            if (!ec) {
                ioc.stop();
            }
        });
        

        // Статическая папка = директория с исполняемым файлом + /static
        auto static_root = std::filesystem::current_path() / "static";

        http_handler::RequestHandler handler{game, static_root};
        http_handler::LoggingRequestHandler logging_handler{handler};
        

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data) << "server started";

        http_server::ServeHttp(ioc, {address, port}, logging_handler);

        RunWorkers(num_threads, [&ioc] { ioc.run(); });

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value{{"code", 0}}) << "server exited";
        return 0;

    } catch (const std::exception& ex) {
        json::object error_data;
        error_data["code"] = EXIT_FAILURE;
        error_data["exception"] = ex.what();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, error_data) << "server exited";
        return EXIT_FAILURE;
    }
}
