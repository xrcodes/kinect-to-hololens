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

    asio::ip::udp::endpoint sender_endpoint{asio::ip::address::from_string(ip_address), gsl::narrow<unsigned short>(port)};
    UdpSocket udp_socket{std::move(socket)};

    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};

    udp_socket.send(create_connect_receiver_packet(receiver_id, true, true).bytes, sender_endpoint);
    bool stopped{false};
    tt::TimePoint last_heartbeat_time{tt::TimePoint::now()};
    tt::TimePoint last_received_any_time{tt::TimePoint::now()};
    tt::TimePoint last_request_time{tt::TimePoint::now()};

    //VideoMessageAssembler video_message_assembler{receiver_id, remote_endpoint};
    AudioPacketReceiver audio_packet_receiver;
    //VideoRenderer video_renderer{receiver_id, remote_endpoint, init_sender_packet_data.width, init_sender_packet_data.height};
    // TODO: Fix to use recieved width and height.
    VideoRenderer video_renderer{receiver_id, sender_endpoint, 640, 576};

    VideoReceiverStorage video_receiver_storage;
    std::map<int, std::shared_ptr<VideoSenderMessage>> video_messages;
    int last_max_storage_frame_id{0};

    for (;;) {
        try {
            if (last_heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                udp_socket.send(create_heartbeat_receiver_packet(receiver_id).bytes, sender_endpoint);
                last_heartbeat_time = tt::TimePoint::now();
            }

            auto sender_packet_info{SenderPacketClassifier::classify(udp_socket)};
            if (sender_packet_info.received_any) {
                //int before_packet_count = video_receiver_storage.getPacketCount();
                for (auto& video_packet : sender_packet_info.video_packets)
                    video_receiver_storage.addVideoPacket(std::make_unique<VideoSenderPacket>(video_packet));

                for (auto& parity_packet : sender_packet_info.parity_packets)
                    video_receiver_storage.addParityPacket(std::make_unique<ParitySenderPacket>(parity_packet));

                //int after_packet_count = video_receiver_storage.getPacketCount();
                //std::cout << "before_packet_count: " << before_packet_count << std::endl;
                //std::cout << "video packet count : " << sender_packet_info.video_packets.size() << std::endl;
                //std::cout << "parity packet count: " << sender_packet_info.parity_packets.size() << std::endl;
                //std::cout << "after_packet_count : " << after_packet_count << std::endl << std::endl;

                bool packet_for_new_frame_exists{false};
                int max_storage_frame_id{video_receiver_storage.getMaxFrameId()};
                if (max_storage_frame_id > last_max_storage_frame_id) {
                    packet_for_new_frame_exists = true;
                    last_max_storage_frame_id = max_storage_frame_id;
                }
                
                video_receiver_storage.build(video_messages);
                //for (auto& [frame_id, _] : video_messages) {
                //    std::cout << "video message built: " << frame_id << std::endl;
                //}

                if (packet_for_new_frame_exists || last_request_time.elapsed_time().sec() > 1.0f) {
                    int request_count{0};
                    auto missing_indices{video_receiver_storage.getMissingIndices()};
                    std::map<int, Packet> request_packets;
                    for (auto& indices : missing_indices) {
                        request_packets.insert({indices.frame_id, create_request_receiver_packet(receiver_id, indices.frame_id, false,
                                                                                                 indices.video_packet_indices,
                                                                                                 indices.parity_packet_indices)});

                        std::cout << "request packets" << std::endl
                                  << "  frame_id: " << indices.frame_id << std::endl
                                  << "  packet count: " << (indices.video_packet_indices.size() + indices.parity_packet_indices.size()) << std::endl;
                    }

                    for (int frame_id = video_renderer.last_frame_id() + 1; frame_id < max_storage_frame_id; ++frame_id) {
                        auto request_packet_it{request_packets.find(frame_id)};
                        if (request_packet_it == request_packets.end()) {
                            request_packets.insert({frame_id, create_request_receiver_packet(receiver_id, frame_id, true,
                                                                                             std::vector<int>(), std::vector<int>())});
                            std::cout << "request whole frame" << std::endl
                                      << "  frame_id: " << frame_id << std::endl;
                        }
                    }
                    for (auto& [_, request_packet] : request_packets) {
                        udp_socket.send(request_packet.bytes, sender_endpoint);
                    }

                    for (auto& [frame_id, set] : video_receiver_storage.frame_parity_sets_) {
                        std::cout << "storage set frame_id: " << frame_id << std::endl
                                  << "  state: " << (int)set.getState() << std::endl
                                  << "  #packets: " << set.getPacketCount() << std::endl
                                  << "  #incorrect groups: " << set.getIncorrectGroupCount() << std::endl
                                  << "  #correctable groups: " << set.getCorrectableGroupCount() << std::endl
                                  << "  #correct groups: " << set.getCorrectGroupCount() << std::endl;
                    }
                    std::cout << "last_frame_id: " << video_renderer.last_frame_id() << std::endl;

                    last_request_time = tt::TimePoint::now();
                }

                audio_packet_receiver.receive(sender_packet_info.audio_packets);
                last_received_any_time = tt::TimePoint::now();
            } else {
                if (last_received_any_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                    std::cout << "Timed out after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                    break;
                }
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError:\n  " << e.what() << "\n";
            break;
        }

        video_renderer.render(udp_socket, video_messages);
        udp_socket.send(create_report_receiver_packet(receiver_id, video_renderer.last_frame_id()).bytes, sender_endpoint);
        //std::cout << "send report: " << video_renderer.last_frame_id() << std::endl;
        video_receiver_storage.removeObsolete(video_renderer.last_frame_id());
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