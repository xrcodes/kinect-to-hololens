#include <iostream>
#include <random>
#include <thread>
#include <readerwriterqueue/readerwriterqueue.h>
#include "native/kh_native.h"
#include "helper/opencv_helper.h"
#include "helper/soundio_helper.h"
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

    //int sender_session_id;
    int width;
    int height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};

    SenderPacketReceiver sender_packet_receiver;
    for (;;) {
        udp_socket.send(create_connect_receiver_packet_bytes(session_id), remote_endpoint);
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        Sleep(300);

        try {
            sender_packet_receiver.receive(udp_socket);
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError while trying to receive InitSenderPacketData:\n  " << e.what() << "\n";
        }
        InitSenderPacketData init_sender_packet_data;
        if (sender_packet_receiver.init_packet_data_queue().try_dequeue(init_sender_packet_data)) {
            width = init_sender_packet_data.width;
            height = init_sender_packet_data.height;
            break;
        }

        if (ping_count == 10) {
            printf("Tried pinging 10 times and failed to received an init packet...\n");
            return;
        }
    }

    bool stopped{false};
    VideoRendererState video_renderer_state;
    VideoMessageAssembler video_message_assembler{session_id, remote_endpoint};

    std::thread task_thread([&] {
        AudioPacketReceiver audio_packet_receiver;

        while (!stopped) {
            sender_packet_receiver.receive(udp_socket);
            video_message_assembler.assemble(udp_socket, sender_packet_receiver.video_packet_data_queue(),
                                             sender_packet_receiver.fec_packet_data_queue(),
                                             video_renderer_state);
            audio_packet_receiver.receive(sender_packet_receiver.audio_packet_data_queue());
        }
    });

    VideoRenderer video_renderer{session_id, remote_endpoint, width, height};
    while (!stopped) {
        video_renderer.render(udp_socket, video_message_assembler.video_message_queue(), video_renderer_state);
    }
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