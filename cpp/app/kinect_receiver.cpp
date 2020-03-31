#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <gsl/gsl>
#include "kh_opus.h"
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "native/kh_time.h"
#include "helper/opencv_helper.h"
#include "helper/soundio_helper.h"
#include <readerwriterqueue/readerwriterqueue.h>
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "receiver/video_renderer.h"
#include "receiver/sender_packet_receiver.h"
#include "receiver/video_message_assembler.h"
#include "receiver/audio_packet_receiver.h"

namespace kh
{
void start_session(const std::string ip_address, const int port, const int session_id)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE = 128 * 1024;
    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});
    asio::ip::udp::endpoint remote_endpoint{asio::ip::address::from_string(ip_address), gsl::narrow_cast<unsigned short>(port)};
    UdpSocket udp_socket{std::move(socket)};

    int sender_session_id;
    int depth_width;
    int depth_height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};
    for (;;) {
        bool initialized{false};
        udp_socket.send(create_connect_receiver_packet_bytes(session_id), remote_endpoint);
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        //Sleep(100);
        Sleep(300);
        
        while (auto packet = udp_socket.receive()) {
            int cursor{0};
            const int session_id{get_session_id_from_sender_packet_bytes(packet->bytes)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(packet->bytes)};
            if (packet_type != SenderPacketType::Init) {
                std::cout << "A different kind of a packet was received before an init packet: " << static_cast<int>(packet_type) << "\n";
                continue;
            }

            sender_session_id = session_id;

            const auto init_sender_packet_data{parse_init_sender_packet_bytes(packet->bytes)};
            depth_width = init_sender_packet_data.width;
            depth_height = init_sender_packet_data.height;

            initialized = true;
            break;
        }
        if (initialized)
            break;

        if (ping_count == 10) {
            printf("Tried pinging 10 times and failed to received an init packet...\n");
            return;
        }
    }

    bool stopped{false};
    VideoRendererState video_renderer_state;
    VideoMessageAssembler video_message_reassembler{session_id, remote_endpoint};

    std::thread task_thread([&] {
        SenderPacketReceiver sender_packet_receiver;
        AudioPacketReceiver audio_packet_collector;

        while (!stopped) {
            sender_packet_receiver.receive(sender_session_id, udp_socket);
            video_message_reassembler.reassemble(udp_socket, sender_packet_receiver.video_packet_data_queue(),
                                                 sender_packet_receiver.fec_packet_data_queue(),
                                                 video_renderer_state);
            audio_packet_collector.collect(sender_packet_receiver.audio_packet_data_queue());
        }
    });

    VideoRenderer video_renderer{session_id, remote_endpoint, depth_width, depth_height};
    video_renderer.render(stopped, udp_socket, video_message_reassembler.video_message_queue(), video_renderer_state);
    stopped = true;

    task_thread.join();
}

void main()
{
    std::mt19937 rng{gsl::narrow_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};
    for (;;) {
        // Receive IP address from the user.
        std::cout << "Enter an IP address to start receiving frames: ";
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        // Receive port from the user.
        std::cout << "Enter a port number to start receiving frames: ";
        std::string port_line;
        std::getline(std::cin, port_line);
        // The default port is 7777.
        const int port{port_line.empty() ? 7777 : std::stoi(port_line)};
        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};

        start_session(ip_address, port, session_id);
    }
}
}

int main()
{
    kh::main();
    return 0;
}