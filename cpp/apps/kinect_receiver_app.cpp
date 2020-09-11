#include <iostream>
#include <random>
#include <thread>
#include "native/kh_native.h"
#include "helper/soundio_helper.h"
#include "receiver/video_renderer.h"
#include "receiver/sender_packet_classifier.h"
#include "receiver/video_message_assembler.h"
#include "receiver/audio_packet_receiver.h"
#include "receiver/video_receiver_storage.h"

namespace kh
{
void start_session(const std::string ip_address, const int port, const int receiver_id)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{5.0f};

    std::cout << "Start kinect_receiver (receiver_id: " << receiver_id << ").\n";

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::v4());
    //std::cout << "endpoint: " << socket.remote_endpoint().port() << "\n";
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});

    asio::ip::udp::endpoint remote_endpoint{asio::ip::address::from_string(ip_address), gsl::narrow<unsigned short>(port)};
    UdpSocket udp_socket{std::move(socket)};

    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};

    udp_socket.send(create_connect_receiver_packet(receiver_id, true, true).bytes, remote_endpoint);
    bool stopped{false};
    tt::TimePoint last_heartbeat_time{tt::TimePoint::now()};
    tt::TimePoint received_any_time{tt::TimePoint::now()};

    //VideoMessageAssembler video_message_assembler{receiver_id, remote_endpoint};
    AudioPacketReceiver audio_packet_receiver;
    //VideoRenderer video_renderer{receiver_id, remote_endpoint, init_sender_packet_data.width, init_sender_packet_data.height};
    // TODO: Fix to use recieved width and height.
    VideoRenderer video_renderer{receiver_id, remote_endpoint, 640, 576};

    VideoReceiverStorage video_receiver_storage;
    std::map<int, std::shared_ptr<VideoSenderMessage>> video_messages;
    int last_max_storage_frame_id{0};

    for (;;) {
        try {
            if (last_heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                udp_socket.send(create_heartbeat_receiver_packet(receiver_id).bytes, remote_endpoint);
                last_heartbeat_time = tt::TimePoint::now();
            }

            auto sender_packet_info{SenderPacketClassifier::classify(udp_socket)};
            if (sender_packet_info.received_any) {
                //video_message_assembler.assemble(udp_socket,
                //                                 sender_packet_info.video_packets,
                //                                 sender_packet_info.parity_packets,
                //                                 video_renderer.last_frame_id(),
                //                                 video_messages);

                for (auto& video_packet : sender_packet_info.video_packets)
                    video_receiver_storage.addVideoPacket(std::make_unique<VideoSenderPacket>(video_packet));

                for (auto& parity_packet : sender_packet_info.parity_packets)
                    video_receiver_storage.addParityPacket(std::make_unique<ParitySenderPacket>(parity_packet));

                bool packet_for_new_frame_exists{false};
                int max_storage_frame_id{video_receiver_storage.getMaxFrameId()};
                if (max_storage_frame_id > last_max_storage_frame_id) {
                    packet_for_new_frame_exists = true;
                    last_max_storage_frame_id = max_storage_frame_id;
                }
                
                video_receiver_storage.build(video_messages);

                if (packet_for_new_frame_exists) {
                    auto missing_indices{video_receiver_storage.getMissingIndices()};
                    // Sending a packet per frame ID.
                    for (auto& indices : missing_indices) {
                        udp_socket.send(create_request_receiver_packet(receiver_id, indices.frame_id,
                                                                       indices.video_packet_indices,
                                                                       indices.parity_packet_indices).bytes,
                                        remote_endpoint);
                    }
                }

                audio_packet_receiver.receive(sender_packet_info.audio_packets);
                received_any_time = tt::TimePoint::now();
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
        video_renderer.render(udp_socket, video_messages);
    }
}

void start()
{
    constexpr int PORT{3773};

    for (;;) {
        // Receive IP address from the user.
        std::cout << "Enter an IP address to start receiving frames: ";
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        const int receiver_id{gsl::narrow<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};

        start_session(ip_address, PORT, receiver_id);
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::start();
    return 0;
}