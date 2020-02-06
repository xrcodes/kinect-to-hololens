#include <chrono>
#include <iostream>
#include <optional>
#include <asio.hpp>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "kh_receiver.h"
#include "kh_frame_packet_collection.h"

namespace kh
{
void _receive_frames(std::string ip_address, int port)
{
    const int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;
    //const int RECEIVER_RECEIVE_BUFFER_SIZE = 64 * 1024;
    asio::io_context io_context;
    Receiver receiver(io_context, RECEIVER_RECEIVE_BUFFER_SIZE);
    receiver.ping(ip_address, port);

    printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

    Vp8Decoder color_decoder;
    int depth_width;
    int depth_height;
    std::unique_ptr<TrvlDecoder> depth_decoder;

    std::unordered_map<int, FramePacketCollection> frame_packet_collections;
    std::vector<FrameMessage> frame_messages;
    int last_frame_id = -1;
    std::optional<int> server_session_id = std::nullopt;

    auto frame_start = std::chrono::steady_clock::now();
    int summary_packet_count = 0;
    for (;;) {
        //auto packet_receive_start = std::chrono::steady_clock::now();
        std::vector<std::vector<uint8_t>> packets;
        for (;;) {
            auto receive_result = receiver.receive();
            if (!receive_result) {
                break;
            }

            // Simulate packet loss
            //if (rand() % 100 == 0) {
            //    continue;
            //}

            packets.push_back(*receive_result);
        }
        if (packets.empty())
            continue;

        summary_packet_count += packets.size();

        //auto packet_collection_start = std::chrono::steady_clock::now();
        for (auto& packet : packets) {
            int cursor = 0;
            int session_id;
            memcpy(&session_id, packet.data() + cursor, 4);
            cursor += 4;

            uint8_t packet_type = packet[cursor];
            cursor += 1;

            if (packet_type == 0) {
                server_session_id = session_id;
                
                // for color width
                cursor += 4;
                // for color height
                cursor += 4;

                memcpy(&depth_width, packet.data() + cursor, 4);
                cursor += 4;

                memcpy(&depth_height, packet.data() + cursor, 4);
                cursor += 4;

                depth_decoder = std::make_unique<TrvlDecoder>(depth_width * depth_height);
            }

            if (!server_session_id || session_id != server_session_id)
                continue;
            
            if (packet_type == 1) {
                int frame_id;
                memcpy(&frame_id, packet.data() + cursor, 4);
                cursor += 4;

                if (frame_id <= last_frame_id)
                    continue;

                int packet_index;
                memcpy(&packet_index, packet.data() + cursor, 4);
                cursor += 4;

                int packet_count;
                memcpy(&packet_count, packet.data() + cursor, 4);
                cursor += 4;

                auto it = frame_packet_collections.find(frame_id);
                if (it == frame_packet_collections.end())
                    frame_packet_collections.insert({ frame_id, FramePacketCollection(frame_id, packet_count) });

                frame_packet_collections.at(frame_id).addPacket(packet_index, packet);
            }
        }

        //printf("Collection Status:\n");
        //for (auto collection_pair : frame_packet_collections) {
        //    int frame_id = collection_pair.first;
        //    auto collected_packet_count = collection_pair.second.getCollectedPacketCount();
        //    auto total_packet_count = collection_pair.second.packet_count();
        //    printf("collection frame_id: %d, collected: %d, total: %d\n", frame_id,
        //           collected_packet_count, total_packet_count);
        //}

        // Find all full collections and their frame_ids.
        std::vector<int> full_frame_ids;
        for (auto& collection_pair : frame_packet_collections) {
            if (collection_pair.second.isFull()) {
                int frame_id = collection_pair.first;
                full_frame_ids.push_back(frame_id);
            }
        }

        // Extract messages from the full collections.
        for (int full_frame_id : full_frame_ids) {
            frame_messages.push_back(frame_packet_collections.at(full_frame_id).toMessage());
            frame_packet_collections.erase(full_frame_id);
        }
        std::sort(frame_messages.begin(), frame_messages.end(), [](const FrameMessage& lhs, const FrameMessage& rhs)
        {
            return lhs.frame_id() < rhs.frame_id();
        });

        if (frame_messages.empty())
            continue;

        for (auto& frame_message : frame_messages) {
            printf("frame_message: %d\n", frame_message.frame_id());
        }

        std::optional<int> begin_index;
        // If there is a key frame, use the most recent one.
        for (int i = frame_messages.size() - 1; i >= 0; --i) {
            if (frame_messages[i].keyframe()) {
                begin_index = i;
                break;
            }
        }

        // When there is no key frame, go through all the frames if the first
        // FrameMessage is the one right after the previously rendered one.
        if (!begin_index) {
            if (frame_messages[0].frame_id() == last_frame_id + 1) {
                begin_index = 0;
            } else {
                // Wait for more frames if there is way to render without glitches.
                continue;
            }
        }

        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        std::chrono::steady_clock::duration packet_collection_time;

        auto decoder_start = std::chrono::steady_clock::now();
        //printf("begin decode\n");
        for (int i = *begin_index; i < frame_messages.size(); ++i) {
            auto frame_message_ptr = &frame_messages[i];

            int frame_id = frame_message_ptr->frame_id();
            //printf("frame id: %d\n", frame_id);
            bool keyframe = frame_message_ptr->keyframe();

            last_frame_id = frame_id;

            packet_collection_time = frame_message_ptr->packet_collection_time();

            auto color_encoder_frame = frame_message_ptr->getColorEncoderFrame();
            auto depth_encoder_frame = frame_message_ptr->getDepthEncoderFrame();

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(color_encoder_frame.data(), color_encoder_frame.size());

            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder->decode(depth_encoder_frame.data(), keyframe);
        }
        auto decoder_time = std::chrono::steady_clock::now() - decoder_start;
        auto frame_time = std::chrono::steady_clock::now() - frame_start;
        frame_start = std::chrono::steady_clock::now();

        receiver.send(last_frame_id,
                      packet_collection_time.count() / 1000000.0f,
                      decoder_time.count() / 1000000.0f,
                      frame_time.count() / 1000000.0f,
                      summary_packet_count);
        summary_packet_count = 0;

        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame->av_frame()));
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()),
                                                         depth_width, depth_height);

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Clean up frame_packet_collections.
        std::vector<int> obsolete_frame_ids;
        for (auto& collection_pair : frame_packet_collections) {
            if (collection_pair.first <= last_frame_id) {
                obsolete_frame_ids.push_back(collection_pair.first);
            }
        }

        for (int obsolete_frame_id : obsolete_frame_ids) {
            frame_packet_collections.erase(obsolete_frame_id);
        }

        // Reset frame_messages after they are displayed.
        frame_messages = std::vector<FrameMessage>();
    }
}

void receive_frames()
{
    for (;;) {
        // Receive IP address from the user.
        printf("Enter an IP address to start receiving frames: ");
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        // Receive port from the user.
        printf("Enter a port number to start receiving frames: ");
        std::string port_line;
        std::getline(std::cin, port_line);
        // The default port is 7777.
        int port = port_line.empty() ? 7777 : std::stoi(port_line);

        try {
            _receive_frames(ip_address, port);
        } catch (std::exception & e) {
            printf("Error from _receive_frames: %s\n", e.what());
        }
    }
}
}

void main()
{
    kh::receive_frames();
}