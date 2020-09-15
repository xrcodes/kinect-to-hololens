#include <iostream>
#include <random>
#include <thread>
#include "native/kh_native.h"
#include "receiver/video_renderer.h"
#include "receiver/sender_packet_classifier.h"
#include "receiver/audio_receiver.h"
#include "receiver/video_receiver_storage.h"

namespace kh
{
namespace
{
std::optional<std::pair<int, std::shared_ptr<VideoSenderMessage>>> find_frame_to_render(std::map<int, std::shared_ptr<VideoSenderMessage>>& video_messages,
                                                                                        std::optional<int> last_frame_id)
{
    if (video_messages.empty())
        return std::nullopt;

    std::optional<int> frame_id_to_render;
    if (!last_frame_id) {
        // For the first frame, find a keyframe.
        for (auto& [frame_id, video_message] : video_messages) {
            if (video_message->keyframe) {
                frame_id_to_render = frame_id;
                break;
            }
        }
    } else {
        // If there is a key frame, use the most recent one.
        for (auto& [frame_id, video_message] : video_messages) {
            if (frame_id <= *last_frame_id)
                continue;

            if (video_message->keyframe) {
                frame_id_to_render = frame_id;
            }
        }

        // Find if there is the next frame.
        if (!frame_id_to_render) {
            auto video_message_it{video_messages.find(*last_frame_id + 1)};
            if (video_message_it != video_messages.end()) {
                frame_id_to_render = *last_frame_id + 1;
            }
        }
    }

    if (!frame_id_to_render)
        return std::nullopt;

    return std::pair<int, std::shared_ptr<VideoSenderMessage>>(*frame_id_to_render, video_messages[*frame_id_to_render]);
}
}

void start_receiver(const std::string ip_address, const unsigned short port, const int receiver_id)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{5.0f};

    std::cout << "Start kinect_receiver (receiver_id: " << receiver_id << ").\n";

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::v4());
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

    AudioReceiver audio_packet_receiver;
    std::unique_ptr<VideoRenderer> video_renderer{nullptr};
    std::optional<int> last_frame_id{std::nullopt};

    VideoReceiverStorage video_receiver_storage;
    std::map<int, std::shared_ptr<VideoSenderMessage>> video_messages;

    for (;;) {
        try {
            if (last_heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                udp_socket.send(create_heartbeat_receiver_packet(receiver_id).bytes, sender_endpoint);
                last_heartbeat_time = tt::TimePoint::now();
            }

            auto sender_packet_info{SenderPacketClassifier::classify(udp_socket)};
            if (sender_packet_info.received_any) {
                for (auto& video_packet : sender_packet_info.video_packets)
                    video_receiver_storage.addVideoPacket(std::make_unique<VideoSenderPacket>(video_packet));

                for (auto& parity_packet : sender_packet_info.parity_packets)
                    video_receiver_storage.addParityPacket(std::make_unique<ParitySenderPacket>(parity_packet));

                video_receiver_storage.build(video_messages);

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

        if (last_request_time.elapsed_time().sec() > 0.1f) {
            auto missing_indices{video_receiver_storage.getMissingIndices()};
            std::map<int, Packet> request_packets;
            for (auto& indices : missing_indices) {
                request_packets.insert({indices.frame_id, create_request_receiver_packet(receiver_id, indices.frame_id, false,
                                                                                            indices.video_packet_indices,
                                                                                            indices.parity_packet_indices)});
            }

            if (last_frame_id) {
                int max_storage_frame_id{video_receiver_storage.getMaxFrameId()};
                for (int frame_id = *last_frame_id + 1; frame_id < max_storage_frame_id; ++frame_id) {
                    auto request_packet_it{request_packets.find(frame_id)};
                    if (request_packet_it == request_packets.end()) {
                        request_packets.insert({frame_id, create_request_receiver_packet(receiver_id, frame_id, true,
                                                                                            std::vector<int>(), std::vector<int>())});
                    }
                }
            }


            std::cout << "request_packets.size(): " << request_packets.size() << std::endl;
            for (auto& [frame_id, request_packet] : request_packets) {
                udp_socket.send(request_packet.bytes, sender_endpoint);
                //std::cout << "sent a request packet: " << frame_id << std::endl;
            }

            last_request_time = tt::TimePoint::now();
        }

        auto frame_with_index{find_frame_to_render(video_messages, last_frame_id)};
        if (frame_with_index) {
            if (!video_renderer)
                video_renderer.reset(new VideoRenderer{frame_with_index->second->width, frame_with_index->second->height});
            video_renderer->render(frame_with_index->second->color_encoder_frame,
                                   frame_with_index->second->depth_encoder_frame,
                                   frame_with_index->second->keyframe);
            last_frame_id = frame_with_index->first;

            // Remove frame messages before and including the rendered frame.
            for (auto it = video_messages.begin(); it != video_messages.end();) {
                if (it->first <= *last_frame_id) {
                    it = video_messages.erase(it);
                } else {
                    ++it;
                }
            }

            udp_socket.send(create_report_receiver_packet(receiver_id, *last_frame_id).bytes, sender_endpoint);
            video_receiver_storage.removeObsolete(*last_frame_id);
        }
    }
}

void start()
{
    constexpr unsigned short PORT{3773};

    for (;;) {
        // Receive IP address from the user.
        std::cout << "Enter an IP address to start receiving frames: ";
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        const int receiver_id{gsl::narrow<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};
        start_receiver(ip_address, PORT, receiver_id);
    }
}
}

int main()
{
    //std::ios_base::sync_with_stdio(false);
    kh::start();
    return 0;
}