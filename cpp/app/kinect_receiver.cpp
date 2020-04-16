#include <iostream>
#include <random>
#include <thread>
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
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{5.0f};

    std::cout << "Start a kinect_receiver session (id: " << session_id << ")\n";

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::v4());
    //std::cout << "endpoint: " << socket.remote_endpoint().port() << "\n";
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});

    asio::ip::udp::endpoint remote_endpoint{asio::ip::address::from_string(ip_address), gsl::narrow_cast<unsigned short>(port)};
    UdpSocket udp_socket{std::move(socket)};

    InitSenderPacketData init_sender_packet_data;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};

    for (;;) {
        udp_socket.send(create_connect_receiver_packet_bytes(session_id), remote_endpoint);
        ++ping_count;
        std::cout << "Sent connect packet to " << ip_address << ".\n";

        Sleep(300);

        try {
            auto sender_packet_set{SenderPacketReceiver::receive(udp_socket)};

            if (!sender_packet_set.init_packet_data_vector.empty()) {
                init_sender_packet_data = sender_packet_set.init_packet_data_vector[0];
                break;
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError while trying to receive InitSenderPacketData:\n  " << e.what() << "\n";
        }

        if (ping_count == 10) {
            printf("Tried pinging 10 times and failed to received an init packet...\n");
            return;
        }
    }

    bool stopped{false};
    TimePoint heartbeat_time{TimePoint::now()};
    TimePoint received_any_time{TimePoint::now()};

    VideoRendererState video_renderer_state;
    VideoMessageAssembler video_message_assembler{session_id, remote_endpoint};
    AudioPacketReceiver audio_packet_receiver;
    VideoRenderer video_renderer{session_id, remote_endpoint, init_sender_packet_data.width, init_sender_packet_data.height};
    std::map<int, VideoSenderMessageData> video_frame_messages;

    for (;;) {
        try {
            if (heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                udp_socket.send(create_heartbeat_receiver_packet_bytes(session_id), remote_endpoint);
                heartbeat_time = TimePoint::now();
            }

            auto sender_packet_set{SenderPacketReceiver::receive(udp_socket)};
            if (sender_packet_set.received_any) {
                video_message_assembler.assemble(udp_socket,
                                                 sender_packet_set.video_packet_data_vector,
                                                 sender_packet_set.fec_packet_data_vector,
                                                 video_renderer_state,
                                                 video_frame_messages);
                audio_packet_receiver.receive(sender_packet_set.audio_packet_data_vector);
                received_any_time = TimePoint::now();
            } else {
                if (received_any_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                    std::cout << "Timed out after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                    break;
                }
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError:\n  " << e.what() << "\n";
            break;
        }
        video_renderer.render(udp_socket, video_renderer_state, video_frame_messages);
    }
}

void main()
{
    constexpr int PORT{47498};

    for (;;) {
        // Receive IP address from the user.
        std::cout << "Enter an IP address to start receiving frames: ";
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        const int session_id{gsl::narrow_cast<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};

        start_session(ip_address, PORT, session_id);
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}