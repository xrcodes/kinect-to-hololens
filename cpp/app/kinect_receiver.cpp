#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>
#include <asio.hpp>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "kh_receiver_socket.h"
#include "kh_video_packet_collection.h"
#include "kh_xor_packet_collection.h"
#include "kh_packet_helper.h"

namespace kh
{
template<class T> using ReaderWriterQueue = moodycamel::ReaderWriterQueue<T>;
using steady_clock = std::chrono::steady_clock;

void run_receiver_thread(int sender_session_id,
                         bool& stop_receiver_thread,
                         ReceiverSocket& receiver,
                         ReaderWriterQueue<VideoMessage>& frame_message_queue,
                         int& last_frame_id,
                         int& summary_packet_count)
{
    constexpr int XOR_MAX_GROUP_SIZE = 5;

    std::unordered_map<int, VideoPacketCollection> frame_packet_collections;
    std::unordered_map<int, XorPacketCollection> xor_packet_collections;
    while (!stop_receiver_thread) {
        std::vector<std::vector<std::byte>> frame_packets;
        std::vector<std::vector<std::byte>> xor_packets;

        std::error_code error;
        while (auto packet = receiver.receive(error)) {
            ++summary_packet_count;

            int cursor = 0;
            int session_id = copy_from_packet<int>(*packet, cursor);
            uint8_t packet_type = copy_from_packet<uint8_t>(*packet, cursor);
            
            if (session_id != sender_session_id)
                continue;

            if (packet_type == 1) {
                frame_packets.push_back(std::move(*packet));
            } else if (packet_type == 2) {
                xor_packets.push_back(std::move(*packet));
            }
        }

        if (error != asio::error::would_block)
            printf("Error from receiving packets: %s\n", error.message().c_str());

        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        for (auto& xor_packet : xor_packets) {
            // Start from 5 since there are 4 bytes for session_id then 1 for packet_type.
            int cursor = 5;
            int frame_id = copy_from_packet<int>(xor_packet, cursor);

            if (frame_id <= last_frame_id)
                continue;

            int packet_index = copy_from_packet<int>(xor_packet, cursor);
            int packet_count = copy_from_packet<int>(xor_packet, cursor);

            if (xor_packet_collections.find(frame_id) == xor_packet_collections.end())
                xor_packet_collections.insert({ frame_id, XorPacketCollection(frame_id, packet_count) });

            xor_packet_collections.at(frame_id).addPacket(packet_index, std::move(xor_packet));
        }

        for (auto& frame_packet : frame_packets) {
            int cursor = 5;
            int frame_id = copy_from_packet<int>(frame_packet, cursor);

            if (frame_id <= last_frame_id)
                continue;

            int packet_index = copy_from_packet<int>(frame_packet, cursor);
            int packet_count = copy_from_packet<int>(frame_packet, cursor);

            // If there is a packet for a new frame, check the previous frames, and if
            // there is a frame with missing packets, try to create them using xor packets.
            // If using the xor packets fails, request the sender to retransmit the packets.
            if (frame_packet_collections.find(frame_id) == frame_packet_collections.end()) {
                frame_packet_collections.insert({ frame_id, VideoPacketCollection(frame_id, packet_count) });

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                for (auto& collection_pair : frame_packet_collections) {
                    if (collection_pair.first < frame_id) {
                        int missing_frame_id = collection_pair.first;
                        auto missing_packet_indices = collection_pair.second.getMissingPacketIndices();

                        // Try correction using XOR FEC packets.
                        std::vector<int> fec_failed_packet_indices;
                        std::vector<int> fec_packet_indices;
                        
                        // missing_packet_index cannot get error corrected if there is another missing_packet_index
                        // that belongs to the same XOR FEC packet...
                        for (int i : missing_packet_indices) {
                            bool found = false;
                            for (int j : missing_packet_indices) {
                                if (i == j)
                                    continue;

                                if ((i / XOR_MAX_GROUP_SIZE) == (j / XOR_MAX_GROUP_SIZE)) {
                                    found = true;
                                    break;
                                }
                            }
                            if (found) {
                                fec_failed_packet_indices.push_back(i);
                            } else {
                                fec_packet_indices.push_back(i);
                            }
                        }

                        for (int fec_packet_index : fec_packet_indices) {
                            // Try getting the XOR FEC packet for correction.
                            int xor_packet_index = fec_packet_index / XOR_MAX_GROUP_SIZE;
                            auto xor_packet_ptr = xor_packet_collections.at(missing_frame_id).TryGetPacket(xor_packet_index);
                            // Give up if there is no xor packet yet.
                            if (!xor_packet_ptr) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            auto fec_start = steady_clock::now();

                            std::vector<std::byte> fec_frame_packet(KH_PACKET_SIZE);

                            uint8_t packet_type = 1;
                            int cursor = 0;
                            copy_to_packet<int>(sender_session_id, fec_frame_packet, cursor);
                            copy_to_packet<uint8_t>(packet_type, fec_frame_packet, cursor);
                            copy_to_packet<int>(missing_frame_id, fec_frame_packet, cursor);
                            copy_to_packet<int>(packet_index, fec_frame_packet, cursor);
                            copy_to_packet<int>(packet_count, fec_frame_packet, cursor);

                            memcpy(fec_frame_packet.data() + cursor, xor_packet_ptr->data() + cursor, KH_MAX_VIDEO_PACKET_CONTENT_SIZE);

                            int begin_frame_packet_index = xor_packet_index * XOR_MAX_GROUP_SIZE;
                            int end_frame_packet_index = std::min(begin_frame_packet_index + XOR_MAX_GROUP_SIZE,
                                                                  collection_pair.second.packet_count());
                            // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                            for (int i = begin_frame_packet_index; i < end_frame_packet_index; ++i) {
                                if (i == fec_packet_index)
                                    continue;

                                for (int j = KH_VIDEO_PACKET_HEADER_SIZE; j < KH_PACKET_SIZE; ++j)
                                    fec_frame_packet[j] ^= collection_pair.second.packets()[i][j];
                            }

                            auto fec_time = steady_clock::now() - fec_start;

                            printf("restored %d %d %lf\n", missing_frame_id, fec_packet_index, fec_time.count() / 1000000.0f);
                            frame_packet_collections.at(missing_frame_id)
                                                    .addPacket(fec_packet_index, std::move(fec_frame_packet));
                        } // end of for (int missing_packet_index : missing_packet_indices)

                        for (int fec_failed_packet_index : fec_failed_packet_indices) {
                            printf("request %d %d\n", missing_frame_id, fec_failed_packet_index);
                        }
                        
                        std::error_code error;
                        receiver.send(missing_frame_id, fec_failed_packet_indices, error);

                        if (error && error != asio::error::would_block)
                            printf("Error requesting missing packets: %s\n", error.message().c_str());
                    }
                }
                /////////////////////////////////
                // Forward Error Correction End//
                /////////////////////////////////
            }
            // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
            // which was for reacting to a packet for a new frame.

            frame_packet_collections.at(frame_id).addPacket(packet_index, std::move(frame_packet));
        }

        // Find all full collections and extract messages from them.
        for (auto it = frame_packet_collections.begin(); it != frame_packet_collections.end();) {
            if (it->second.isFull()) {
                frame_message_queue.enqueue(it->second.toMessage());
                it = frame_packet_collections.erase(it);
            } else {
                ++it;
            }
        }

        if(!frame_packet_collections.empty())
            printf("Collection Status:\n");

        for (auto& collection_pair : frame_packet_collections) {
            int frame_id = collection_pair.first;
            auto collected_packet_count = collection_pair.second.getCollectedPacketCount();
            auto total_packet_count = collection_pair.second.packet_count();
            printf("collection frame_id: %d, collected: %d, total: %d\n", frame_id,
                   collected_packet_count, total_packet_count);
        }

        // Clean up frame_packet_collections.
        for (auto it = frame_packet_collections.begin(); it != frame_packet_collections.end();) {
            if (it->first <= last_frame_id) {
                it = frame_packet_collections.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up xor_packet_collections.
        for (auto it = xor_packet_collections.begin(); it != xor_packet_collections.end();) {
            if (it->first <= last_frame_id) {
                it = xor_packet_collections.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void receive_frames(std::string ip_address, int port)
{
    const int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;
    //const int RECEIVER_RECEIVE_BUFFER_SIZE = 64 * 1024;
    asio::io_context io_context;
    ReceiverSocket receiver(io_context, RECEIVER_RECEIVE_BUFFER_SIZE);

    std::error_code error;
    int sender_session_id;
    int depth_width;
    int depth_height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count = 0;
    for (;;) {
        bool initialized = false;
        receiver.ping(ip_address, port);
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        Sleep(100);
        
        while (auto packet = receiver.receive(error)) {
            int cursor = 0;
            int session_id = copy_from_packet<int>(*packet, cursor);
            uint8_t packet_type = copy_from_packet<uint8_t>(*packet, cursor);
            if (packet_type != KH_SENDER_INIT_PACKET) {
                printf("A different kind of a packet received before an init packet: %d\n", packet_type);
                continue;
            }

            sender_session_id = session_id;

            //int color_width = copy_from_packet<int>(packet, cursor);
            //int color_height = copy_from_packet<int>(packet, cursor);
            cursor += 8;

            depth_width = copy_from_packet<int>(*packet, cursor);
            depth_height = copy_from_packet<int>(*packet, cursor);

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

    bool stop_receiver_thread = false;
    moodycamel::ReaderWriterQueue<VideoMessage> frame_message_queue;
    int last_frame_id = -1;
    int summary_packet_count = 0;
    std::thread receiver_thread(run_receiver_thread, sender_session_id, std::ref(stop_receiver_thread),
                                std::ref(receiver), std::ref(frame_message_queue),
                                std::ref(last_frame_id), std::ref(summary_packet_count));

    Vp8Decoder color_decoder;
    TrvlDecoder depth_decoder(depth_width * depth_height);

    std::vector<VideoMessage> frame_messages;
    auto frame_start = steady_clock::now();
    for (;;) {
        VideoMessage frame_message;
        while (frame_message_queue.try_dequeue(frame_message)) {
            frame_messages.push_back(frame_message);
        }

        std::sort(frame_messages.begin(), frame_messages.end(), [](const VideoMessage& lhs, const VideoMessage& rhs)
        {
            return lhs.frame_id() < rhs.frame_id();
        });

        //for (auto& frame_message : frame_messages) {
        //    printf("frame_message: %d\n", frame_message.frame_id());
        //}

        if (frame_messages.empty())
            continue;

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
        steady_clock::duration packet_collection_time;

        auto decoder_start = steady_clock::now();
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
            depth_image = depth_decoder.decode(depth_encoder_frame.data(), keyframe);
        }
        auto decoder_time = steady_clock::now() - decoder_start;
        auto frame_time = steady_clock::now() - frame_start;
        frame_start = steady_clock::now();

        std::error_code error;
        receiver.send(last_frame_id,
                      packet_collection_time.count() / 1000000.0f,
                      decoder_time.count() / 1000000.0f,
                      frame_time.count() / 1000000.0f,
                      summary_packet_count,
                      error);

        if (error && error != asio::error::would_block)
            printf("Error sending receiver status: %s\n", error.message().c_str());

        summary_packet_count = 0;

        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame->av_frame()));
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()),
                                                         depth_width, depth_height);

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Reset frame_messages after they are displayed.
        frame_messages = std::vector<VideoMessage>();
    }

    stop_receiver_thread = true;
    receiver_thread.join();
}

void main()
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
        receive_frames(ip_address, port);
    }
}
}

int main()
{
    kh::main();
    return 0;
}