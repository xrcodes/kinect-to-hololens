#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "native/kh_native.h"
#include "sender/audio_packet_sender.h"
#include "sender/kinect_device_manager.h"
#include "sender/receiver_packet_receiver.h"

namespace kh
{
// Update receiver_state and summary with Report packets.
void apply_report_packets(std::vector<ReportReceiverPacketData>& report_packet_data_vector,
                          ReceiverState& receiver_state,
                          ReceiverReportSummary& summary)
{
    // Update receiver_state and summary with Report packets.
    for (auto& report_receiver_packet_data : report_packet_data_vector) {
        // Ignore if network is somehow out of order and a report comes in out of order.
        if (report_receiver_packet_data.frame_id <= receiver_state.video_frame_id)
            continue;

        receiver_state.video_frame_id = report_receiver_packet_data.frame_id;

        summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
        summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
        ++summary.received_report_count;
    }
}

void print_receiver_report_summary(ReceiverReportSummary summary, TimeDuration duration)
{
    std::cout << "Receiver Reported in " << summary.received_report_count / duration.sec() << " Hz\n"
              << "  Decoder Time Average: " << summary.decoder_time_ms_sum / summary.received_report_count << " ms\n"
              << "  Frame Interval Time Average: " << summary.frame_interval_ms_sum / summary.received_report_count << " ms\n";
}

void print_kinect_device_manager_summary(KinectDeviceManagerSummary summary, TimeDuration duration)
{
    std::cout << "KinectDeviceManager Summary:\n"
              << "  Frame ID: " << summary.frame_id << "\n"
              << "  FPS: " << summary.frame_count / duration.sec() << "\n"
              << "  Bandwidth: " << summary.byte_count / duration.sec() / (1024.0f * 1024.0f / 8.0f) << " Mbps\n"
              << "  Keyframe Ratio: " << static_cast<float>(summary.keyframe_count) / summary.frame_count << " ms\n"
              << "  Shadow Removal Time Average: " << summary.shadow_removal_ms_sum / summary.frame_count << " ms\n"
              << "  Transformation Time Average: " << summary.transformation_ms_sum / summary.frame_count << " ms\n"
              << "  Yuv Conversion Time Average: " << summary.yuv_conversion_ms_sum / summary.frame_count << " ms\n"
              << "  Color Encoder Time Average: " << summary.color_encoder_ms_sum / summary.frame_count << " ms\n"
              << "  Depth Encoder Time Average: " << summary.depth_encoder_ms_sum / summary.frame_count << " ms\n";
}

void start_session(const int port, const int session_id)
{
    constexpr int SENDER_SEND_BUFFER_SIZE{128 * 1024};

    std::cout << "Start a kinect_sender session (id: " << session_id << ")\n";

    KinectDevice kinect_device;
    kinect_device.start();

    // Create UdpSocket.
    asio::io_context io_context;
    asio::ip::udp::socket socket{io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)};
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});
    UdpSocket udp_socket{std::move(socket)};

    // Receive a connect packet from a receiver and capture the receiver's endpoint.
    // Then, create ReceiverState with it.
    asio::ip::udp::endpoint receiver_endpoint;
    for (;;) {
        auto receiver_packet_set{ReceiverPacketReceiver::receive(udp_socket)};

        if (!receiver_packet_set.connect_endpoint_vector.empty()) {
            receiver_endpoint = receiver_packet_set.connect_endpoint_vector[0];
            break;
        }
    }
    ReceiverState receiver_state{receiver_endpoint};

    std::cout << "Found a Receiver at " << receiver_state.endpoint << ".\n";

    // Initialize instances for loop below.
    const TimePoint session_start_time{TimePoint::now()};

    KinectDeviceManager kinect_device_manager{session_id, receiver_state.endpoint, std::move(kinect_device)};
    KinectDeviceManagerSummary kinect_device_manager_summary;

    KinectAudioSender kinect_audio_sender{session_id, receiver_state.endpoint};

    VideoPacketRetransmitter video_packet_retransmitter{session_id, receiver_state.endpoint};
    ReceiverReportSummary receiver_report_summary;

    VideoParityPacketStorage video_parity_packet_storage;

    // Run the loop.
    for (;;) {
        try {
            kinect_device_manager.update(session_start_time, udp_socket, video_parity_packet_storage, receiver_state, kinect_device_manager_summary);
            kinect_audio_sender.send(udp_socket);

            auto receiver_packet_set{ReceiverPacketReceiver::receive(udp_socket)};
            apply_report_packets(receiver_packet_set.report_packet_data_vector, receiver_state, receiver_report_summary);
            video_packet_retransmitter.retransmit(udp_socket, receiver_packet_set.request_packet_data_vector, video_parity_packet_storage);
            video_parity_packet_storage.cleanup(receiver_state.video_frame_id);
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError:\n  " << e.what() << "\n";
            break;
        }

        const auto summary_duration{receiver_report_summary.start_time.elapsed_time()};
        if (summary_duration.sec() > 10.0f) {
            print_receiver_report_summary(receiver_report_summary, summary_duration);
            receiver_report_summary = ReceiverReportSummary{};

            print_kinect_device_manager_summary(kinect_device_manager_summary, summary_duration);
            kinect_device_manager_summary = KinectDeviceManagerSummary{};
        }
    }
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void main()
{
    std::mt19937 rng{gsl::narrow_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};
    for (;;) {
        std::string line;
        std::cout << "Enter a port number to start sending frames: ";
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        const int port{line.empty() ? 7777 : std::stoi(line)};
        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};

        start_session(port, session_id);
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}
