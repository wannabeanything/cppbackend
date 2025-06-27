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
#include <boost/program_options.hpp>
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

namespace
{
    template <typename Fn>
    void RunWorkers(unsigned n, const Fn &fn)
    {
        n = std::max(1u, n);
        std::vector<std::thread> workers;
        workers.reserve(n - 1);
        while (--n)
        {
            workers.emplace_back(fn);
        }
        fn();
        for (auto &worker : workers)
        {
            worker.join();
        }
    }
}

int main(int argc, const char *argv[])
{
    namespace po = boost::program_options;

    po::options_description desc("Allowed options");
    desc.add_options()
    ("help,h", "produce help message")
    ("tick-period,t", po::value<int>(), "milliseconds set tick period")
    ("config-file,c", po::value<std::string>(), "file set config file path")
    ("www-root,w", po::value<std::string>(), "dir set static files root")
    ("randomize-spawn-points", "spawn dogs at random positions");

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error parsing command line: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    std::optional<std::chrono::milliseconds> tick_period;
    if (vm.count("tick-period")) {
        tick_period = std::chrono::milliseconds(vm["tick-period"].as<int>());
    }
    if (vm.count("help") || !vm.count("config-file") || !vm.count("www-root"))
    {
        std::cout << desc << std::endl;
        return EXIT_SUCCESS;
    }

    const std::string config_file = vm["config-file"].as<std::string>();
    const std::string www_root = vm["www-root"].as<std::string>();
    const bool randomize_spawn = vm.count("randomize-spawn-points") > 0;

    try
    {
        http_handler::InitLogging();
        
        model::Game game = json_loader::LoadGame(config_file);

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&ioc](const sys::error_code &ec, int)
                           {
            if (!ec) {
                ioc.stop();
            } });

        // Статическая папка = директория с исполняемым файлом + /static
        auto static_root = www_root;
        net::strand<net::io_context::executor_type> api_strand = net::make_strand(ioc);
        http_handler::RequestHandler handler{game, static_root, api_strand};
        http_handler::LoggingRequestHandler logging_handler{handler};

        const auto address = net::ip::make_address("0.0.0.0");
        constexpr net::ip::port_type port = 8080;

        json::object start_data;
        start_data["port"] = port;
        start_data["address"] = address.to_string();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, start_data) << "server started";

        http_server::ServeHttp(ioc, {address, port},
                               [&logging_handler](auto &&req, auto &&send, const std::string &ip)
                               {
                                   logging_handler(std::forward<decltype(req)>(req),
                                                   std::forward<decltype(send)>(send),
                                                   ip);
                               });

        RunWorkers(num_threads, [&ioc]
                   { ioc.run(); });

        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, json::value{{"code", 0}}) << "server exited";
        return 0;
    }
    catch (const std::exception &ex)
    {
        json::object error_data;
        error_data["code"] = EXIT_FAILURE;
        error_data["exception"] = ex.what();
        BOOST_LOG_TRIVIAL(info) << logging::add_value(additional_data, error_data) << "server exited";
        return EXIT_FAILURE;
    }
}
