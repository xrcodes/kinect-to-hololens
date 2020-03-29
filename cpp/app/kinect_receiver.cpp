#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <random>
#include <thread>
#include <gsl/gsl>
#include "kh_opus.h"
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "native/kh_time.h"
#include "helper/opencv_helper.h"
#include "helper/soundio_helper.h"
#include "receiver_modules.h"

namespace kh
{
void consume_video_message(bool& stopped,
                           const int session_id,
                           const asio::ip::udp::endpoint remote_endpoint,
                           int depth_width,
                           int depth_height,
                           UdpSocket& udp_socket,
                           moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>>& video_message_queue,
                           int& last_video_frame_id)
{
    Vp8Decoder color_decoder;
    TrvlDecoder depth_decoder{depth_width * depth_height};

    std::map<int, VideoSenderMessageData> frame_messages;
    auto frame_start{TimePoint::now()};
    while (!stopped) {
        std::pair<int, VideoSenderMessageData> frame_message;
        while (video_message_queue.try_dequeue(frame_message)) {
            frame_messages.insert(std::move(frame_message));
        }

        if (frame_messages.empty())
            continue;

        std::optional<int> begin_frame_id;
        // If there is a key frame, use the most recent one.
        for (auto& frame_message_pair : frame_messages) {
            if (frame_message_pair.first <= last_video_frame_id)
                continue;

            if (frame_message_pair.second.keyframe)
                begin_frame_id = frame_message_pair.first;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!begin_frame_id) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            if (frame_messages.find(last_video_frame_id + 1) != frame_messages.end()) {
                begin_frame_id = last_video_frame_id + 1;
            } else {
                // Wait for more frames if there is way to render without glitches.
                continue;
            }
        }

        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        const auto decoder_start{TimePoint::now()};
        for (int i = *begin_frame_id; ; ++i) {
            // break loop is there is no frame with frame_id i.
            if (frame_messages.find(i) == frame_messages.end())
                break;

            const auto frame_message_pair_ptr{&frame_messages[i]};

            last_video_frame_id = i;

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(frame_message_pair_ptr->color_encoder_frame);
            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder.decode(frame_message_pair_ptr->depth_encoder_frame, frame_message_pair_ptr->keyframe);
        }

        udp_socket.send(create_report_receiver_packet_bytes(session_id,
                                                            last_video_frame_id,
                                                            decoder_start.elapsed_time().ms(),
                                                            frame_start.elapsed_time().ms()), remote_endpoint);
        frame_start = TimePoint::now();

        auto color_mat{create_cv_mat_from_yuv_image(createYuvImageFromAvFrame(*ffmpeg_frame->av_frame()))};
        auto depth_mat{create_cv_mat_from_kinect_depth_image(depth_image.data(), depth_width, depth_height)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Remove frame messages before the rendered frame.
        for (auto it = frame_messages.begin(); it != frame_messages.end();) {
            if (it->first < last_video_frame_id) {
                it = frame_messages.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void start_session(const std::string ip_address, const int port, const int session_id)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE = 128 * 1024;
    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});
    asio::ip::udp::endpoint remote_endpoint{asio::ip::address::from_string(ip_address), gsl::narrow_cast<unsigned short>(port)};
    UdpSocket udp_socket{std::move(socket)};

    int sender_session_id;
    int depth_width;
    int depth_height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};
    for (;;) {
        bool initialized{false};
        udp_socket.send(create_connect_receiver_packet_bytes(session_id), remote_endpoint);
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        //Sleep(100);
        Sleep(300);
        
        while (auto packet = udp_socket.receive()) {
            int cursor{0};
            const int session_id{get_session_id_from_sender_packet_bytes(packet->bytes)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(packet->bytes)};
            if (packet_type != SenderPacketType::Init) {
                std::cout << "A different kind of a packet was received before an init packet: " << static_cast<int>(packet_type) << "\n";
                continue;
            }

            sender_session_id = session_id;

            const auto init_sender_packet_data{parse_init_sender_packet_bytes(packet->bytes)};
            depth_width = init_sender_packet_data.width;
            depth_height = init_sender_packet_data.height;

            initialized = true;
            break;
        }
        if (initialized)
            break;

        if (ping_count == 10) {
            printf("Tried pinging 10 times and failed to received an init packet...\n");
            return;
        }
    }

    bool stopped{false};
    int last_video_frame_id{-1};

    VideoMessageReassembler video_message_reassembler{session_id, remote_endpoint};

    std::thread task_thread([&] {
        SenderPacketReceiver sender_packet_receiver;
        AudioPacketCollector audio_packet_collector;

        while (!stopped) {
            sender_packet_receiver.receive(sender_session_id, udp_socket);
            video_message_reassembler.reassemble(udp_socket, sender_packet_receiver.video_packet_data_queue(),
                                                 sender_packet_receiver.fec_packet_data_queue(),
                                                 last_video_frame_id);
            audio_packet_collector.collect(sender_packet_receiver.audio_packet_data_queue());
        }
    });

    consume_video_message(stopped, session_id, remote_endpoint, depth_width, depth_height, udp_socket, video_message_reassembler.video_message_queue(), last_video_frame_id);
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