#define WIN32_LEAN_AND_MEAN
#include "audio.h"
#include <iostream>
#include <boost/asio.hpp>

using namespace std::literals;
using boost::asio::ip::udp;

// Function to start the server
void StartServer(uint16_t port) {
    try {
        boost::asio::io_context io_context;
        udp::socket socket(io_context, udp::endpoint(udp::v4(), port));
        Player player(ma_format_u8, 1);

        std::cout << "Server started on port " << port << std::endl;

        while (true) {
            udp::endpoint sender_endpoint;
            std::vector<char> buffer(65000); 

            
            size_t length = socket.receive_from(boost::asio::buffer(buffer), sender_endpoint);
            std::cout << "Received " << length << " bytes from " << sender_endpoint << std::endl;

            
            size_t frames = length / player.GetFrameSize();

            
            player.PlayBuffer(buffer.data(), frames, 1.5s);
            std::cout << "Playback done" << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in server: " << e.what() << std::endl;
    }
}


void StartClient(uint16_t port) {
    try {
        boost::asio::io_context io_context;
        udp::socket socket(io_context, udp::v4());
        Recorder recorder(ma_format_u8, 1);

        std::cout << "Client started. Enter server IP address:" << std::endl;

        while (true) {
            std::string ip_address;
            std::getline(std::cin, ip_address);

            if (ip_address.empty()) {
                std::cout << "Invalid IP address. Try again." << std::endl;
                continue;
            }
            if(ip_address == "done")break;
            udp::resolver resolver(io_context);
            udp::endpoint server_endpoint = *resolver.resolve(udp::v4(), ip_address, std::to_string(port)).begin();

            std::cout << "Press Enter to record and send a message..." << std::endl;
            std::string str;
            std::getline(std::cin, str);

            
            auto rec_result = recorder.Record(65000, 1.5s);
            std::cout << "Recording done. Sending " << rec_result.frames << " frames to server..." << std::endl;

            
            size_t bytes_to_send = rec_result.frames * recorder.GetFrameSize();

            
            socket.send_to(boost::asio::buffer(rec_result.data.data(), bytes_to_send), server_endpoint);
            std::cout << "Message sent to server." << std::endl;
        }
    } catch (std::exception& e) {
        std::cerr << "Exception in client: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " [client|server] <port>" << std::endl;
        return 1;
    }

    std::string mode(argv[1]);
    uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));

    if (mode == "server") {
        StartServer(port);
    } else if (mode == "client") {
        StartClient(port);
    } else {
        std::cerr << "Invalid mode. Use 'client' or 'server'." << std::endl;
        return 1;
    }

    return 0;
}