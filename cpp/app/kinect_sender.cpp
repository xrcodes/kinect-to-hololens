#include <iostream>
#include <random>
#include <tuple>
#include "native/kh_native.h"
#include "sender/kinect_audio_sender.h"
#include "sender/kinect_video_sender.h"
#include "sender/receiver_packet_receiver.h"

namespace kh
{
std::optional<KinectDevice> create_and_start_kinect_device()
{
    try {
        KinectDevice kinect_device;
        kinect_device.start();
        return kinect_device;
    } catch (k4a::error e) {
        return std::nullopt;
    }
}

// Update receiver_state and summary with Report packets.
void apply_report_packets(std::vector<ReportReceiverPacketData>& report_packet_data_vector,
                          RemoteReceiver& remote_receiver,
                          ReceiverReportSummary& summary)
{
    // Update receiver_state and summary with Report packets.
    for (auto& report_receiver_packet_data : report_packet_data_vector) {
        // Ignore if network is somehow out of order and a report comes in out of order.
        if (report_receiver_packet_data.frame_id <= remote_receiver.video_frame_id)
            continue;

        remote_receiver.video_frame_id = report_receiver_packet_data.frame_id;

        summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
        summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
        ++summary.received_report_count;
    }
}

void retransmit_requested_packets(UdpSocket& udp_socket,
                                  std::vector<RequestReceiverPacketData>& request_packet_data_vector,
                                  VideoParityPacketStorage& video_parity_packet_storage,
                                  const asio::ip::udp::endpoint remote_endpoint)
{
    // Retransmit the requested video packets.
    for (auto& request_receiver_packet_data : request_packet_data_vector) {
        const int frame_id{request_receiver_packet_data.frame_id};
        if (!video_parity_packet_storage.has(frame_id))
            continue;

        for (int packet_index : request_receiver_packet_data.video_packet_indices)
            udp_socket.send(video_parity_packet_storage.get(frame_id).video_packet_byte_set[packet_index], remote_endpoint);

        for (int packet_index : request_receiver_packet_data.parity_packet_indices)
            udp_socket.send(video_parity_packet_storage.get(frame_id).parity_packet_byte_set[packet_index], remote_endpoint);
    }
}

void print_receiver_report_summary(ReceiverReportSummary summary, TimeDuration duration)
{
    std::cout << "Receiver Reported in " << summary.received_report_count / duration.sec() << " Hz\n"
              << "  Decoder Time Average: " << summary.decoder_time_ms_sum / summary.received_report_count << " ms\n"
              << "  Frame Interval Time Average: " << summary.frame_interval_ms_sum / summary.received_report_count << " ms\n";
}

void print_kinect_video_sender_summary(KinectVideoSenderSummary summary, TimeDuration duration)
{
    std::cout << "KinectDeviceManager Summary:\n"
              << "  Frame ID: " << summary.frame_id << "\n"
              << "  FPS: " << summary.frame_count / duration.sec() << "\n"
              << "  Color Bandwidth: " << summary.color_byte_count / duration.sec() / (1024.0f * 1024.0f / 8.0f) << " Mbps\n"
              << "  Depth Bandwidth: " << summary.depth_byte_count / duration.sec() / (1024.0f * 1024.0f / 8.0f) << " Mbps\n"
              << "  Keyframe Ratio: " << static_cast<float>(summary.keyframe_count) / summary.frame_count << " ms\n"
              << "  Shadow Removal Time Average: " << summary.shadow_removal_ms_sum / summary.frame_count << " ms\n"
              << "  Transformation Time Average: " << summary.transformation_ms_sum / summary.frame_count << " ms\n"
              << "  Yuv Conversion Time Average: " << summary.yuv_conversion_ms_sum / summary.frame_count << " ms\n"
              << "  Color Encoder Time Average: " << summary.color_encoder_ms_sum / summary.frame_count << " ms\n"
              << "  Depth Encoder Time Average: " << summary.depth_encoder_ms_sum / summary.frame_count << " ms\n";
}

void main()
{
    constexpr int PORT{47498};
    constexpr int SENDER_SEND_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC{3.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{10.0f};
    constexpr float SUMMARY_INTERVAL_SEC{10.0f};

    // The default port (the port when nothing is entered) is 7777.
    const int session_id{gsl::narrow_cast<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};

    std::cout << "Start kinect_sender (session_id: " << session_id << ").\n";

    std::optional<KinectDevice> kinect_device{create_and_start_kinect_device()};
    if (!kinect_device) {
        std::cout << "Failed to create and start the Kinect device.\n";
        return;
    }

    // Create UdpSocket.
    asio::io_context io_context;
    asio::ip::udp::socket socket{io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), PORT)};
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});
    UdpSocket udp_socket{std::move(socket)};

    asio::ip::udp::resolver resolver(io_context);
    asio::ip::udp::resolver::query query(asio::ip::host_name(), "");
    auto resolver_results = resolver.resolve(query);
    std::cout << "local endpoints:\n";
    for (auto it = resolver_results.begin(); it != resolver_results.end(); ++it) {
        if (it->endpoint().protocol() != asio::ip::udp::v4())
            continue;
        std::cout << "  - " << it->endpoint().address() << "\n";
    }

    // Initialize instances for loop below.
    const TimePoint session_start_time{TimePoint::now()};
    TimePoint heartbeat_time{TimePoint::now()};

    KinectVideoSender kinect_video_sender{session_id, std::move(*kinect_device)};
    KinectVideoSenderSummary kinect_video_sender_summary;

    KinectAudioSender kinect_audio_sender{session_id};
    
    ReceiverReportSummary receiver_report_summary;

    VideoParityPacketStorage video_parity_packet_storage;

    std::unordered_map<int, RemoteReceiver> remote_receivers;

    // Run the loop.
    for (;;) {
        try {
            std::vector<int> receiver_session_ids;
            for (auto& [receiver_session_id, _] : remote_receivers)
                receiver_session_ids.push_back(receiver_session_id);
            
            auto receiver_packet_collection = ReceiverPacketReceiver::receive(udp_socket, receiver_session_ids);

            // Receive a connect packet from a receiver and capture the receiver's endpoint.
            // Then, create ReceiverState with it.
            for (auto& connect_packet_info : receiver_packet_collection.connect_packet_infos) {
                // Skip already existing receivers.
                if (remote_receivers.find(connect_packet_info.session_id) != remote_receivers.end())
                    continue;

                std::cout << "Receiver " << connect_packet_info.session_id << " connected.\n";
                remote_receivers.insert({connect_packet_info.session_id, RemoteReceiver{connect_packet_info.endpoint, connect_packet_info.session_id}});
            }

            // Skip the main part of the loop if there is no receiver connected.
            if (!remote_receivers.empty()) {
                std::vector<asio::ip::udp::endpoint> remote_endpoints;
                for (auto& [_, remote_receiver] : remote_receivers)
                    remote_endpoints.push_back(remote_receiver.endpoint);

                // Send video/audio packets to the receivers.
                kinect_video_sender.send(session_start_time, udp_socket, video_parity_packet_storage, remote_receivers, kinect_video_sender_summary);
                kinect_audio_sender.send(udp_socket, remote_endpoints);

                // Send heartbeat packets to receivers.
                if (heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                    for (auto& remote_endpoint : remote_endpoints)
                        udp_socket.send(create_heartbeat_sender_packet_bytes(session_id), remote_endpoint);
                    heartbeat_time = TimePoint::now();
                }

                for (auto& [receiver_session_id, receiver_packet_set] : receiver_packet_collection.receiver_packet_sets) {
                    auto remote_receiver_ptr{&remote_receivers.at(receiver_session_id)};
                    if (receiver_packet_set.received_any) {
                        apply_report_packets(receiver_packet_set.report_packet_data_vector,
                                             *remote_receiver_ptr,
                                             receiver_report_summary);
                        retransmit_requested_packets(udp_socket,
                                                     receiver_packet_set.request_packet_data_vector,
                                                     video_parity_packet_storage,
                                                     remote_receiver_ptr->endpoint);
                        remote_receiver_ptr->last_packet_time = TimePoint::now();
                    } else {
                        if (remote_receiver_ptr->last_packet_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                            std::cout << "Timed out receiver " << receiver_session_id << " after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                            remote_receivers.erase(receiver_session_id);
                        }
                    }
                }
            }

            video_parity_packet_storage.cleanup(VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC);
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError\n  message: " << e.what() << "\n  endpoint: " << e.endpoint() << "\n";
            std::cout << "remote_receivers.size(): " << remote_receivers.size() << "\n";
            for (auto it{remote_receivers.begin()}; it != remote_receivers.end();) {
                if (it->second.endpoint == e.endpoint()) {
                    it = remote_receivers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        const auto summary_duration{receiver_report_summary.time_point.elapsed_time()};
        if (summary_duration.sec() > SUMMARY_INTERVAL_SEC) {
            print_receiver_report_summary(receiver_report_summary, summary_duration);
            receiver_report_summary = ReceiverReportSummary{};

            print_kinect_video_sender_summary(kinect_video_sender_summary, summary_duration);
            kinect_video_sender_summary = KinectVideoSenderSummary{};
        }
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}
