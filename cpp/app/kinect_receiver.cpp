#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "native/kh_receiver_socket.h"
#include "native/kh_video_packet_collection.h"
#include "native/kh_fec_packet_collection.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"

namespace kh
{
void run_receiver_thread(int sender_session_id,
                         bool& stop_receiver_thread,
                         ReceiverSocket& receiver,
                         moodycamel::ReaderWriterQueue<VideoMessage>& frame_message_queue,
                         int& last_frame_id,
                         int& summary_packet_count)
{
    constexpr int FEC_MAX_GROUP_SIZE{5};

    std::unordered_map<int, VideoPacketCollection> frame_packet_collections;
    std::unordered_map<int, FecPacketCollection> fec_packet_collections;
    while (!stop_receiver_thread) {
        std::vector<std::vector<std::byte>> frame_packet_bytes_vector;
        std::vector<std::vector<std::byte>> fec_packet_bytes_vector;

        std::error_code error;
        while (auto packet{receiver.receive(error)}) {
            ++summary_packet_count;

            const int session_id{get_session_id_from_sender_packet_bytes(*packet)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(*packet)};

            if (session_id != sender_session_id)
                continue;

            if (packet_type == SenderPacketType::Video) {
                frame_packet_bytes_vector.push_back(std::move(*packet));
            } else if (packet_type == SenderPacketType::Fec) {
                fec_packet_bytes_vector.push_back(std::move(*packet));
            }
        }

        if (error != asio::error::would_block)
            printf("Error from receiving packets: %s\n", error.message().c_str());

        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        for (auto& fec_packet_bytes : fec_packet_bytes_vector) {
            // Start from 5 since there are 4 bytes for session_id then 1 for packet_type.
            const auto fec_sender_packet_data{parse_fec_sender_packet_bytes(fec_packet_bytes)};

            if (fec_sender_packet_data.frame_id <= last_frame_id)
                continue;

            if (fec_packet_collections.find(fec_sender_packet_data.frame_id) == fec_packet_collections.end())
                fec_packet_collections.insert({fec_sender_packet_data.frame_id,
                                               FecPacketCollection{fec_sender_packet_data.frame_id,
                                                                   fec_sender_packet_data.packet_count}});

            fec_packet_collections.at(fec_sender_packet_data.frame_id)
                                  .addPacket(fec_sender_packet_data.packet_index, std::move(fec_packet_bytes));
        }

        for (auto& frame_packet_bytes : frame_packet_bytes_vector) {
            const auto frame_sender_packet_data{parse_fec_sender_packet_bytes(frame_packet_bytes)};

            if (frame_sender_packet_data.frame_id <= last_frame_id)
                continue;

            // If there is a packet for a new frame, check the previous frames, and if
            // there is a frame with missing packets, try to create them using xor packets.
            // If using the xor packets fails, request the sender to retransmit the packets.
            if (frame_packet_collections.find(frame_sender_packet_data.frame_id) == frame_packet_collections.end()) {
                frame_packet_collections.insert({frame_sender_packet_data.frame_id,
                                                 VideoPacketCollection(frame_sender_packet_data.frame_id,
                                                                       frame_sender_packet_data.packet_count)});

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                for (auto& collection_pair : frame_packet_collections) {
                    if (collection_pair.first < frame_sender_packet_data.frame_id) {
                        const int missing_frame_id{collection_pair.first};
                        const auto missing_packet_indices{collection_pair.second.getMissingPacketIndices()};

                        // Try correction using XOR FEC packets.
                        std::vector<int> fec_failed_packet_indices;
                        std::vector<int> fec_packet_indices;
                        
                        // missing_packet_index cannot get error corrected if there is another missing_packet_index
                        // that belongs to the same XOR FEC packet...
                        for (int i : missing_packet_indices) {
                            bool found{false};
                            for (int j : missing_packet_indices) {
                                if (i == j)
                                    continue;

                                if ((i / FEC_MAX_GROUP_SIZE) == (j / FEC_MAX_GROUP_SIZE)) {
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
                            const int fec_packet_index{fec_packet_index / FEC_MAX_GROUP_SIZE};

                            if (fec_packet_collections.find(missing_frame_id) == fec_packet_collections.end()) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            const auto fec_packet_ptr{fec_packet_collections.at(missing_frame_id).TryGetPacket(fec_packet_index)};
                            // Give up if there is no xor packet yet.
                            if (!fec_packet_ptr) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            const auto fec_start{TimePoint::now()};

                            std::vector<std::byte> fec_frame_packet(KH_PACKET_SIZE);

                            auto packet_type{SenderPacketType::Video};
                            PacketCursor cursor;
                            copy_to_bytes(sender_session_id, fec_frame_packet, cursor);
                            copy_to_bytes(packet_type, fec_frame_packet, cursor);
                            copy_to_bytes(missing_frame_id, fec_frame_packet, cursor);
                            copy_to_bytes(frame_sender_packet_data.packet_index, fec_frame_packet, cursor);
                            copy_to_bytes(frame_sender_packet_data.packet_count, fec_frame_packet, cursor);

                            memcpy(fec_frame_packet.data() + cursor.position,
                                   fec_packet_ptr->data() + cursor.position,
                                   KH_MAX_VIDEO_PACKET_CONTENT_SIZE);

                            const int begin_frame_packet_index{fec_packet_index * FEC_MAX_GROUP_SIZE};
                            const int end_frame_packet_index{std::min(begin_frame_packet_index + FEC_MAX_GROUP_SIZE,
                                                                      collection_pair.second.packet_count())};
                            // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                            for (gsl::index i = begin_frame_packet_index; i < end_frame_packet_index; ++i) {
                                if (i == fec_packet_index)
                                    continue;

                                for (gsl::index j = KH_VIDEO_PACKET_HEADER_SIZE; j < KH_PACKET_SIZE; ++j)
                                    fec_frame_packet[j] ^= collection_pair.second.packets()[i][j];
                            }

                            const auto fec_time{TimePoint::now() - fec_start};

                            //printf("restored %d %d %lf\n", missing_frame_id, fec_packet_index, fec_time.count() / 1000000.0f);
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

            frame_packet_collections.at(frame_sender_packet_data.frame_id)
                                    .addPacket(frame_sender_packet_data.packet_index, std::move(frame_packet_bytes));
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

        //if(!frame_packet_collections.empty())
        //    printf("Collection Status:\n");

        //for (auto& collection_pair : frame_packet_collections) {
        //    const int frame_id{collection_pair.first};
        //    const auto collected_packet_count{collection_pair.second.getCollectedPacketCount()};
        //    const auto total_packet_count{collection_pair.second.packet_count()};
        //    printf("collection frame_id: %d, collected: %d, total: %d\n", frame_id,
        //           collected_packet_count, total_packet_count);
        //}

        // Clean up frame_packet_collections.
        for (auto it = frame_packet_collections.begin(); it != frame_packet_collections.end();) {
            if (it->first <= last_frame_id) {
                it = frame_packet_collections.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up xor_packet_collections.
        for (auto it = fec_packet_collections.begin(); it != fec_packet_collections.end();) {
            if (it->first <= last_frame_id) {
                it = fec_packet_collections.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void receive_frames(std::string ip_address, int port)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;
    asio::io_context io_context;
    ReceiverSocket receiver{io_context, RECEIVER_RECEIVE_BUFFER_SIZE};

    std::error_code error;
    int sender_session_id;
    int depth_width;
    int depth_height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};
    for (;;) {
        bool initialized{false};
        receiver.ping(ip_address, port);
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        Sleep(100);
        
        while (auto packet = receiver.receive(error)) {
            int cursor{0};
            const int session_id{get_session_id_from_sender_packet_bytes(*packet)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(*packet)};
            if (packet_type != SenderPacketType::Init) {
                printf("A different kind of a packet received before an init packet: %d\n", packet_type);
                continue;
            }

            sender_session_id = session_id;;

            const auto init_sender_packet_data{parse_init_sender_packet_bytes(*packet)};
            depth_width = init_sender_packet_data.depth_width;
            depth_height = init_sender_packet_data.depth_height;

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

    bool stop_receiver_thread{false};
    moodycamel::ReaderWriterQueue<VideoMessage> frame_message_queue;
    int last_frame_id{-1};
    int summary_packet_count{0};
    std::thread receiver_thread([&] {
        run_receiver_thread(sender_session_id, stop_receiver_thread, receiver, frame_message_queue,
                            last_frame_id, summary_packet_count);
    });

    Vp8Decoder color_decoder;
    TrvlDecoder depth_decoder{depth_width * depth_height};

    std::map<int, VideoMessage> frame_messages;
    auto frame_start{TimePoint::now()};
    for (;;) {
        VideoMessage frame_message;
        while (frame_message_queue.try_dequeue(frame_message)) {
            //frame_messages.push_back(frame_message);
            frame_messages.insert({frame_message.frame_id(), frame_message});
        }

        if (frame_messages.empty())
            continue;

        std::optional<int> begin_frame_id;
        // If there is a key frame, use the most recent one.
        for (auto& frame_message_pair : frame_messages) {
            if (frame_message_pair.first <= last_frame_id)
                continue;

            if (frame_message_pair.second.keyframe())
                begin_frame_id = frame_message_pair.first;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!begin_frame_id) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            if(frame_messages.find(last_frame_id + 1) != frame_messages.end()){
                begin_frame_id = last_frame_id + 1;
            } else {
                // Wait for more frames if there is way to render without glitches.
                continue;
            }
        }

        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        TimeDuration packet_collection_time;

        const auto decoder_start{TimePoint::now()};
        for (int i = *begin_frame_id; ; ++i) {
            // break loop is there is no frame with frame_id i.
            if (frame_messages.find(i) == frame_messages.end())
                break;

            const auto frame_message_ptr{&frame_messages[i]};

            const int frame_id{frame_message_ptr->frame_id()};
            //printf("frame id: %d\n", frame_id);
            const bool keyframe{frame_message_ptr->keyframe()};

            last_frame_id = frame_id;

            packet_collection_time = frame_message_ptr->packet_collection_time();

            const auto color_encoder_frame{frame_message_ptr->getColorEncoderFrame()};
            const auto depth_encoder_frame{frame_message_ptr->getDepthEncoderFrame()};

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(color_encoder_frame);

            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder.decode(depth_encoder_frame, keyframe);
        }
        const auto decoder_time{TimePoint::now() - decoder_start};
        const auto frame_time{TimePoint::now() - frame_start};
        frame_start = TimePoint::now();

        std::error_code error;
        receiver.send(last_frame_id,
                      packet_collection_time.ms(),
                      decoder_time.ms(),
                      frame_time.ms(),
                      summary_packet_count,
                      error);

        if (error && error != asio::error::would_block)
            printf("Error sending receiver status: %s\n", error.message().c_str());

        summary_packet_count = 0;

        auto color_mat{createCvMatFromYuvImage(createYuvImageFromAvFrame(*ffmpeg_frame->av_frame()))};
        auto depth_mat{createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()),
                                                       depth_width,
                                                       depth_height)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Remove frame messages before the rendered frame.
        for (auto it = frame_messages.begin(); it != frame_messages.end();) {
            if (it->first < last_frame_id) {
                it = frame_messages.erase(it);
            } else {
                ++it;
            }
        }
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
        const int port{port_line.empty() ? 7777 : std::stoi(port_line)};
        receive_frames(ip_address, port);
    }
}
}

int main()
{
    kh::main();
    return 0;
}