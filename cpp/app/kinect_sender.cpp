#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "native/kh_native.h"
#include "helper/shadow_remover.h"
#include "sender/video_packet_sender.h"
#include "sender/audio_packet_sender.h"
#include "sender/kinect_device_manager.h"
#include "sender/receiver_packet_receiver.h"

namespace kh
{
void print_video_packet_sender_summary(VideoPacketSenderSummary summary, TimeDuration duration)
{
    std::cout << "Receiver Reported in " << summary.received_report_count / duration.sec() << " Hz\n"
              << "  Decoder Time Average: " << summary.decoder_time_ms_sum / summary.received_report_count << " ms\n"
              << "  Frame Interval Time Average: " << summary.frame_interval_ms_sum / summary.received_report_count << " ms\n";
}

void print_kinect_device_manager_summary(KinectDeviceManagerSummary summary, TimeDuration duration)
{
    std::cout << "Video Frame Summary:\n"
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

    std::cout << "start kinect_sender session " << session_id << "\n";

    KinectDevice kinect_device;
    kinect_device.start();

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});
    UdpSocket udp_socket{std::move(socket)};

    ReceiverPacketReceiver receiver_packet_receiver;
    asio::ip::udp::endpoint receiver_endpoint;
    for (;;) {
        receiver_packet_receiver.receive(udp_socket);
        if (receiver_packet_receiver.connect_endpoint_queue().try_dequeue(receiver_endpoint))
            break;
    }

    std::cout << "Found a Receiver at " << receiver_endpoint << "\n";

    const TimePoint session_start_time{TimePoint::now()};

    bool stopped{false};
    moodycamel::ReaderWriterQueue<VideoFecPacketByteSet> video_fec_packet_byte_set_queue;
    ReceiverState receiver_state;
    std::thread task_thread([&] {
        try {
            VideoPacketSender video_packet_sender{session_id, receiver_endpoint};
            VideoPacketSenderSummary video_packet_sender_summary;

            AudioPacketSender audio_packet_sender{session_id, receiver_endpoint};
            while (!stopped) {
                receiver_packet_receiver.receive(udp_socket);
                video_packet_sender.send(udp_socket, receiver_packet_receiver.report_packet_data_queue(),
                                         receiver_packet_receiver.request_packet_data_queue(), video_fec_packet_byte_set_queue, receiver_state, video_packet_sender_summary);
                audio_packet_sender.send(udp_socket);

                const auto summary_duration{video_packet_sender_summary.start_time.elapsed_time()};
                if (summary_duration.sec() > 10.0f) {
                    print_video_packet_sender_summary(video_packet_sender_summary, summary_duration);
                    video_packet_sender_summary = VideoPacketSenderSummary{};
                }
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError from task_thread:\n  " << e.what() << "\n";
        }
        stopped = true;
    });

    KinectDeviceManager kinect_device_manager{session_id, receiver_endpoint, std::move(kinect_device)};
    KinectDeviceManagerSummary kinect_device_manager_summary;
    try {
        while (!stopped) {
            kinect_device_manager.update(session_start_time, stopped, udp_socket, video_fec_packet_byte_set_queue, receiver_state, kinect_device_manager_summary);

            const auto summary_duration{kinect_device_manager_summary.start_time.elapsed_time()};
            if (summary_duration.sec() > 10.0f) {
                print_kinect_device_manager_summary(kinect_device_manager_summary, summary_duration);
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
