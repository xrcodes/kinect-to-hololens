#include <chrono>
#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/kinect_helper.h"
#include "native/kh_udp_socket.h"
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"

namespace kh
{
// Pair of the frame's id and its packets.
using VideoPacketSet = std::pair<int, std::vector<std::vector<std::byte>>>;

void handle_receiver_messages(int session_id,
                              bool& stop_threads,
                              UdpSocket& sender_socket,
                              moodycamel::ReaderWriterQueue<VideoPacketSet>& video_packet_queue,
                              int& receiver_frame_id)
{
    constexpr int XOR_MAX_GROUP_SIZE = 5;

    std::unordered_map<int, TimePoint> video_frame_send_times;
    std::unordered_map<int, VideoPacketSet> video_packet_sets;
    int last_receiver_video_frame_id{0};
    auto video_sender_summary_start{TimePoint::now()};
    int video_sender_summary_receiver_frame_count{0};
    int video_sender_summary_receiver_packet_count{0};
    int video_sender_summary_packet_count{0};
    while (!stop_threads) {
        for (;;) {
            std::error_code error;
            std::optional<std::vector<std::byte>> received_packet{sender_socket.receive(error)};

            if (!received_packet) {
                if (error == asio::error::would_block) {
                    break;
                } else {
                    printf("Error receving a packet: %s\n", error.message().c_str());
                    goto run_video_sender_thread_end;
                }
            }

            const auto message_type{get_packet_type_from_receiver_packet_bytes(*received_packet)};

            if (message_type == ReceiverPacketType::Report) {
                const auto report_receiver_packet_data{parse_report_receiver_packet_bytes(*received_packet)};
                receiver_frame_id = report_receiver_packet_data.frame_id;

                const auto round_trip_time{TimePoint::now() - video_frame_send_times[receiver_frame_id]};

                printf("Frame id: %d, decoder: %f ms, frame: %f ms, round_trip: %f ms\n",
                       receiver_frame_id,
                       report_receiver_packet_data.decoder_time_ms,
                       report_receiver_packet_data.frame_time_ms,
                       round_trip_time.ms());

                for (auto it = video_frame_send_times.begin(); it != video_frame_send_times.end();) {
                    if (it->first <= receiver_frame_id) {
                        it = video_frame_send_times.erase(it);
                    } else {
                        ++it;
                    }
                }

                ++video_sender_summary_receiver_frame_count;
                video_sender_summary_receiver_packet_count += report_receiver_packet_data.packet_count;
            } else if (message_type == ReceiverPacketType::Request) {
                const auto request_receiver_packet_data{parse_request_receiver_packet_bytes(*received_packet)};

                for (int packet_index : request_receiver_packet_data.packet_indices) {
                    if (video_packet_sets.find(request_receiver_packet_data.frame_id) == video_packet_sets.end())
                        continue;

                    sender_socket.send(video_packet_sets[request_receiver_packet_data.frame_id].second[packet_index], error);
                    if (error == asio::error::would_block) {
                        printf("Failed to fill in a packet as the buffer was full...\n");
                    } else if (error) {
                        printf("Error while filling in a packet: %s\n", error.message().c_str());
                        goto run_video_sender_thread_end;
                    }

                    ++video_sender_summary_packet_count;
                }
            }
        }

        VideoPacketSet video_packet_set;
        while (video_packet_queue.try_dequeue(video_packet_set)) {
            auto xor_packets{create_fec_sender_packet_bytes_vector(session_id, video_packet_set.first, XOR_MAX_GROUP_SIZE, video_packet_set.second)};

            video_frame_send_times.insert({video_packet_set.first, TimePoint::now()});
            for (auto& packet : video_packet_set.second) {
                std::error_code error;
                sender_socket.send(packet, error);

                if (error == asio::error::would_block) {
                    printf("Failed to send a frame packet as the buffer was full...\n");
                } else if (error) {
                    printf("Error from sending a frame packet: %s\n", error.message().c_str());
                    goto run_video_sender_thread_end;
                }

                ++video_sender_summary_packet_count;
            }

            for (auto packet : xor_packets) {
                std::error_code error;
                sender_socket.send(packet, error);

                if (error == asio::error::would_block) {
                    printf("Failed to send an xor packet as the buffer was full...\n");
                } else if (error) {
                    printf("Error from sending an xor packet: %s\n", error.message().c_str());
                    goto run_video_sender_thread_end;
                }

                ++video_sender_summary_packet_count;
            }
            video_packet_sets.insert({video_packet_set.first, std::move(video_packet_set)});
        }

        // Remove elements of frame_packet_sets reserved for filling up missing packets
        // if they are already used from the receiver side.
        for (auto it = video_packet_sets.begin(); it != video_packet_sets.end();) {
            if (it->first <= receiver_frame_id) {
                it = video_packet_sets.erase(it);
            } else {
                ++it;
            }
        }

        if ((receiver_frame_id / 100) > (last_receiver_video_frame_id / 100)) {
            const auto send_summary_time_interval{TimePoint::now() - video_sender_summary_start};
            const float packet_loss{1.0f - video_sender_summary_receiver_packet_count / static_cast<float>(video_sender_summary_packet_count)};
            printf("Send Summary: Receiver FPS: %lf, Packet Loss: %f%%\n",
                   video_sender_summary_receiver_frame_count / send_summary_time_interval.sec(),
                   gsl::narrow_cast<float>(packet_loss * 100.0f));

            video_sender_summary_start = TimePoint::now();
            video_sender_summary_receiver_frame_count = 0;
            video_sender_summary_packet_count = 0;
            video_sender_summary_receiver_packet_count = 0;
        }
        last_receiver_video_frame_id = receiver_frame_id;
    }
run_video_sender_thread_end:
    stop_threads = true;
    return;
}

void send_frames(int port, int session_id, KinectDevice& kinect_device)
{
    constexpr int TARGET_BITRATE = 2000;
    constexpr short CHANGE_THRESHOLD = 10;
    constexpr int INVALID_THRESHOLD = 2;
    constexpr int SENDER_SEND_BUFFER_SIZE = 1024 * 1024;
    //const int SENDER_SEND_BUFFER_SIZE = 128 * 1024;

    printf("Start Sending Frames (session_id: %d, port: %d)\n", session_id, port);

    const auto calibration{kinect_device.getCalibration()};
    k4a::transformation transformation{calibration};

    const int depth_width{calibration.depth_camera_calibration.resolution_width};
    const int depth_height{calibration.depth_camera_calibration.resolution_height};
    
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    Vp8Encoder color_encoder{depth_width, depth_height, TARGET_BITRATE};
    TrvlEncoder depth_encoder{depth_width * depth_height, CHANGE_THRESHOLD, INVALID_THRESHOLD};

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});

    std::vector<std::byte> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);
    if (error) {
        printf("Error receiving ping: %s\n", error.message().c_str());
        return;
    }

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    UdpSocket sender_socket{std::move(socket), remote_endpoint};

    bool stop_threads{false};
    moodycamel::ReaderWriterQueue<VideoPacketSet> video_packet_queue;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id{-1};
    std::thread video_sender_thread([&] {
        handle_receiver_messages(session_id, stop_threads, sender_socket, video_packet_queue, receiver_frame_id);
    });

    // frame_id is the ID of the frame the sender sends.
    int video_frame_id{0};

    // Variables for profiling the sender.
    int main_summary_keyframe_count{0};
    auto last_device_time_stamp{std::chrono::microseconds::zero()};

    auto main_summary_start{TimePoint::now()};
    size_t main_summary_frame_size_sum{0};
    for (;;) {
        // Stop if the sender thread stopped.
        if (stop_threads)
            break;

        if (receiver_frame_id == -1) {
            const auto init_packet_bytes{create_init_sender_packet_bytes(session_id, create_init_sender_packet_data(calibration))};
            //sender_socket.sendInitPacket(session_id, calibration, error);
            sender_socket.send(init_packet_bytes, error);
            if (error) {
                printf("Error sending init packet: %s\n", error.message().c_str());
                return;
            }
            Sleep(100);
        }

        const auto capture{kinect_device.getCapture()};
        if (!capture)
            continue;

        const auto color_image{capture->get_color_image()};
        if (!color_image) {
            printf("get_color_image() failed...\n");
            continue;
        }

        const auto depth_image{capture->get_depth_image()};
        if (!depth_image) {
            printf("get_depth_image() failed...\n");
            continue;
        }

        const auto device_time_stamp{color_image.get_device_timestamp()};
        const auto device_time_diff{device_time_stamp - last_device_time_stamp};
        const int device_frame_diff{static_cast<int>(device_time_diff.count() / 33000.0f + 0.5f)};
        const int frame_id_diff{video_frame_id - receiver_frame_id};
        if (device_frame_diff < static_cast<int>(std::pow(2, frame_id_diff - 3)))
            continue;

        last_device_time_stamp = device_time_stamp;

        const bool keyframe{frame_id_diff > 5};

        const auto transformed_color_image{transformation.color_image_to_depth_camera(depth_image, color_image)};

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        const auto yuv_image{createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                     transformed_color_image.get_width_pixels(),
                                                                     transformed_color_image.get_height_pixels(),
                                                                     transformed_color_image.get_stride_bytes())};
        const auto vp8_frame{color_encoder.encode(yuv_image, keyframe)};

        // Compress the depth pixels.
        //const auto depth_encoder_frame{depth_encoder.encode(reinterpret_cast<const int16_t*>(depth_image.get_buffer()), keyframe)};
        const auto depth_encoder_frame{depth_encoder.encode({reinterpret_cast<const int16_t*>(depth_image.get_buffer()),
                                                             gsl::narrow_cast<ptrdiff_t>(depth_image.get_size())},
                                                            keyframe)};

        const float frame_time_stamp{device_time_stamp.count() / 1000.0f};
        //const auto message{SenderSocket::createFrameMessage(frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
        const auto message{create_video_sender_message_bytes(frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
        //auto packets{SenderSocket::createFramePackets(session_id, video_frame_id, message)};
        auto packets{split_video_sender_message_bytes(session_id, video_frame_id, message)};
        video_packet_queue.enqueue({video_frame_id, std::move(packets)});

        // Updating variables for profiling.
        if (keyframe)
            ++main_summary_keyframe_count;
        main_summary_frame_size_sum += (vp8_frame.size() + depth_encoder_frame.size());

        // Print profile measures every 100 frames.
        if (video_frame_id % 100 == 0) {
            const auto main_summary_time_interval{TimePoint::now() - main_summary_start};
            printf("Main Summary id: %d, FPS: %lf, Keyframe Ratio: %d%%, Bandwidth: %lf Mbps\n",
                   video_frame_id,
                   100 / main_summary_time_interval.sec(),
                   main_summary_keyframe_count,
                   main_summary_frame_size_sum / (main_summary_time_interval.sec() * 131072));

            main_summary_start = TimePoint::now();
            main_summary_keyframe_count = 0;
            main_summary_frame_size_sum = 0;
        }
        ++video_frame_id;
    }
    stop_threads = true;
    video_sender_thread.join();
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void main()
{
    srand(time(nullptr));
    std::mt19937 rng{gsl::narrow_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};

    for (;;) {
        std::string line;
        printf("Enter a port number to start sending frames: ");
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        const int port{line.empty() ? 7777 : std::stoi(line)};
        
        k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
        configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
        configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
        configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        constexpr auto timeout = std::chrono::milliseconds(1000);

        //const int session_id(rng() % (INT_MAX + 1));
        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};
        
        auto kinect_device{KinectDevice::create(configuration, timeout)};
        if (!kinect_device) {
            printf("Failed to create a KinectDevice...\n");
            continue;
        }
        kinect_device->start();

        send_frames(port, session_id, *kinect_device);
    }
}
}

int main()
{
    kh::main();
    return 0;
}