#include <chrono>
#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "kh_opus.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "native/kh_kinect_device.h"
#include "helper/soundio_helper.h"
#include "helper/shadow_remover.h"
#include <vector>
#include "sender/video_packet_sender.h"
#include "sender/audio_packet_sender.h"
#include "PointCloudGenerator.h"
#include "FloorDetector.h"
#include "sender/kinect_device_manager.h"

namespace kh
{
void start_session(int port, int session_id, KinectDevice& kinect_device)
{
    constexpr int SENDER_SEND_BUFFER_SIZE = 128 * 1024;

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});

    std::vector<std::byte> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);
    if (error)
        throw std::runtime_error(std::string("Error receiving ping: ") + error.message());

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    UdpSocket udp_socket{std::move(socket), remote_endpoint};

    bool stopped{false};
    moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>> video_packet_queue;
    ReceiverState receiver_state;
    std::thread task_thread([&] {
        try {
            VideoPacketSender video_packet_sender;
            AudioPacketSender audio_packet_sender;
            
            VideoPacketSenderSummary video_packet_sender_summary;
            while (!stopped) {
                video_packet_sender.send(session_id, udp_socket, video_packet_queue, receiver_state, video_packet_sender_summary);
                audio_packet_sender.send(session_id, udp_socket);

                const TimeDuration summary_duration{TimePoint::now() - video_packet_sender_summary.start_time};
                if (summary_duration.sec() > 10.0f) {
                    std::cout << "Receiver Reported in " << video_packet_sender_summary.received_report_count / summary_duration.sec() << " Hz\n"
                        << "  Decoder Time Average: " << video_packet_sender_summary.decoder_time_ms_sum / video_packet_sender_summary.received_report_count << " ms\n"
                        << "  Frame Interval Time Average: " << video_packet_sender_summary.frame_interval_ms_sum / video_packet_sender_summary.received_report_count << " ms\n"
                        << "  Round Trip Time Average: " << video_packet_sender_summary.round_trip_ms_sum / video_packet_sender_summary.received_report_count << " ms\n";
                    video_packet_sender_summary = VideoPacketSenderSummary{};
                }
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError from task_thread:\n  " << e.what() << "\n";
        }
        stopped = true;
    });

    TimePoint session_start_time{TimePoint::now()};
    KinectDeviceManagerSummary kinect_device_manager_summary;
    KinectDeviceManager kinect_device_manager{std::move(kinect_device)};
    try {
        while (!stopped) {
            kinect_device_manager.update(session_id, session_start_time, stopped, udp_socket, video_packet_queue, receiver_state, kinect_device_manager_summary);

            const TimeDuration summary_duration{TimePoint::now() - kinect_device_manager_summary.start_time};
            if (summary_duration.sec() > 10.0f) {
                std::cout << "Video Frame Summary:\n"
                    << "  Frame ID: " << kinect_device_manager_summary.frame_id << "\n"
                    << "  FPS: " << kinect_device_manager_summary.frame_count / summary_duration.sec() << "\n"
                    << "  Bandwidth: " << kinect_device_manager_summary.byte_count / summary_duration.sec() / (1024.0f * 1024.0f / 8.0f) << " Mbps\n"
                    << "  Keyframe Ratio: " << static_cast<float>(kinect_device_manager_summary.keyframe_count) / kinect_device_manager_summary.frame_count << " ms\n"
                    << "  Shadow Removal Time Average: " << kinect_device_manager_summary.shadow_removal_ms_sum / kinect_device_manager_summary.frame_count << " ms\n"
                    << "  Transformation Time Average: " << kinect_device_manager_summary.transformation_ms_sum / kinect_device_manager_summary.frame_count << " ms\n"
                    << "  Yuv Conversion Time Average: " << kinect_device_manager_summary.yuv_conversion_ms_sum / kinect_device_manager_summary.frame_count << " ms\n"
                    << "  Color Encoder Time Average: " << kinect_device_manager_summary.color_encoder_ms_sum / kinect_device_manager_summary.frame_count << " ms\n"
                    << "  Depth Encoder Time Average: " << kinect_device_manager_summary.depth_encoder_ms_sum / kinect_device_manager_summary.frame_count << " ms\n";
                kinect_device_manager_summary = KinectDeviceManagerSummary{};
            }
        }

    } catch (UdpSocketRuntimeError e) {
        std::cout << "UdpSocketRuntimeError from send_video_frames(): \n  " << e.what() << "\n";
    }
    stopped = true;

    task_thread.join();
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void main()
{
    srand(time(nullptr));
    std::mt19937 rng{gsl::narrow_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};

    for (;;) {
        std::string line;
        std::cout << "Enter a port number to start sending frames: ";
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        const int port{line.empty() ? 7777 : std::stoi(line)};
        
        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};
        
        KinectDevice kinect_device;
        kinect_device.start();

        start_session(port, session_id, kinect_device);
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}
