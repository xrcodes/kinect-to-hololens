#include <chrono>
#include <iostream>
#include <optional>
#include <thread>
#include <asio.hpp>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "kh_receiver.h"
#include "kh_frame_packet_collection.h"

namespace kh
{
int int_min(int x, int y)
{
    return x < y ? x : y;
}

class XorPacketCollection
{
public:
    XorPacketCollection(int frame_id, int packet_count)
        : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count)
    {
    }
    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }
    void addPacket(int packet_index, std::vector<uint8_t>&& packet)
    {
        packets_[packet_index] = std::move(packet);
    }
    std::vector<std::uint8_t>* TryGetPacket(int packet_index)
    {
        if (packets_[packet_index].empty())
        {
            return nullptr;
        }

        return &packets_[packet_index];
    }

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::vector<std::uint8_t>> packets_;
};

void run_receiver_thread(bool& stop_receiver_thread,
                         Receiver& receiver,
                         moodycamel::ReaderWriterQueue<std::vector<uint8_t>>& init_packet_queue,
                         moodycamel::ReaderWriterQueue<FrameMessage>& frame_message_queue,
                         int& last_frame_id,
                         int& summary_packet_count)
{
    const int XOR_MAX_GROUP_SIZE = 5;

    std::optional<int> sender_session_id = std::nullopt;
    std::unordered_map<int, FramePacketCollection> frame_packet_collections;
    std::unordered_map<int, XorPacketCollection> xor_packet_collections;
    while (!stop_receiver_thread) {
        std::vector<std::vector<uint8_t>> frame_packets;
        std::vector<std::vector<uint8_t>> xor_packets;
        for (;;) {
            auto packet = receiver.receive();
            if (!packet) {
                break;
            }

            int session_id;
            memcpy(&session_id, packet->data(), 4);
            uint8_t packet_type = (*packet)[4];

            if (packet_type == 0) {
                sender_session_id = session_id;
                init_packet_queue.enqueue(*packet);
            }
            
            if (!sender_session_id || session_id != sender_session_id)
                continue;

            if (packet_type == 1) {
                frame_packets.push_back(std::move(*packet));
            } else if (packet_type == 2) {
                xor_packets.push_back(std::move(*packet));
            }

            ++summary_packet_count;
        }

        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        for (auto& xor_packet : xor_packets) {
            int cursor = 5;

            int frame_id;
            memcpy(&frame_id, xor_packet.data() + cursor, 4);
            cursor += 4;

            if (frame_id <= last_frame_id)
                continue;

            int packet_index;
            memcpy(&packet_index, xor_packet.data() + cursor, 4);
            cursor += 4;

            int packet_count;
            memcpy(&packet_count, xor_packet.data() + cursor, 4);
            //cursor += 4;

            auto it = xor_packet_collections.find(frame_id);
            if (it == xor_packet_collections.end()) {
                xor_packet_collections.insert({ frame_id, XorPacketCollection(frame_id, packet_count) });
            }

            xor_packet_collections.at(frame_id).addPacket(packet_index, std::move(xor_packet));
        }

        for (auto& frame_packet : frame_packets) {
            int cursor = 5;

            int frame_id;
            memcpy(&frame_id, frame_packet.data() + cursor, 4);
            cursor += 4;

            if (frame_id <= last_frame_id)
                continue;

            int packet_index;
            memcpy(&packet_index, frame_packet.data() + cursor, 4);
            cursor += 4;

            int packet_count;
            memcpy(&packet_count, frame_packet.data() + cursor, 4);
            //cursor += 4;

            auto it = frame_packet_collections.find(frame_id);
            if (it == frame_packet_collections.end()) {
                frame_packet_collections.insert({ frame_id, FramePacketCollection(frame_id, packet_count) });

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                for (auto& collection_pair : frame_packet_collections) {
                    if (collection_pair.first < frame_id) {
                        int missing_frame_id = collection_pair.first;
                        auto missing_packet_indices = collection_pair.second.getMissingPacketIds();

                        // Try correction using XOR FEC packets.
                        std::vector<int> fec_failed_packet_indices;
                        
                        // missing_packet_index cannot get error corrected if there is another missing_packet_index
                        // that belongs to the same XOR FEC packet...
                        for (int i : missing_packet_indices) {
                            for (int j : missing_packet_indices) {
                                if (i == j)
                                    continue;
                                if ((i / XOR_MAX_GROUP_SIZE) == (j / XOR_MAX_GROUP_SIZE)) {
                                    fec_failed_packet_indices.push_back(i);
                                    continue;
                                }
                            }
                        }

                        for (int missing_packet_index : missing_packet_indices) {
                            // If fec_failed_packet_indices already contains missing_packet_index, skip.
                            if (std::find(fec_failed_packet_indices.begin(), fec_failed_packet_indices.end(), missing_packet_index) != fec_failed_packet_indices.end()) {
                                continue;
                            }

                            // Try getting the XOR FEC packet for correction.
                            int xor_packet_index = missing_packet_index / XOR_MAX_GROUP_SIZE;
                            auto xor_packet_ptr = xor_packet_collections.at(missing_frame_id).TryGetPacket(xor_packet_index);
                            // Give up if there is no xor packet yet.
                            if (!xor_packet_ptr) {
                                fec_failed_packet_indices.push_back(missing_packet_index);
                                continue;
                            }

                            auto fec_start = std::chrono::steady_clock::now();

                            const int PACKET_SIZE = 1500;
                            const int PACKET_HEADER_SIZE = 17;
                            const int MAX_PACKET_CONTENT_SIZE = PACKET_SIZE - PACKET_HEADER_SIZE;
                            std::vector<uint8_t> fec_frame_packet(PACKET_SIZE);

                            uint8_t packet_type = 1;
                            int cursor = 0;
                            memcpy(fec_frame_packet.data() + cursor, &(*sender_session_id), 4);
                            cursor += 4;

                            memcpy(fec_frame_packet.data() + cursor, &packet_type, 1);
                            cursor += 1;

                            memcpy(fec_frame_packet.data() + cursor, &missing_frame_id, 4);
                            cursor += 4;

                            memcpy(fec_frame_packet.data() + cursor, &packet_index, 4);
                            cursor += 4;

                            memcpy(fec_frame_packet.data() + cursor, &packet_count, 4);
                            cursor += 4;

                            memcpy(fec_frame_packet.data() + cursor, xor_packet_ptr->data() + cursor, MAX_PACKET_CONTENT_SIZE);

                            int begin_frame_packet_index = xor_packet_index * XOR_MAX_GROUP_SIZE;
                            int end_frame_packet_index = int_min(begin_frame_packet_index + XOR_MAX_GROUP_SIZE, collection_pair.second.packet_count());
                            // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                            for (int i = begin_frame_packet_index; i < end_frame_packet_index; ++i) {
                                if (i == missing_packet_index) {
                                    continue;
                                }

                                for (int j = PACKET_HEADER_SIZE; j < PACKET_SIZE; ++j) {
                                    fec_frame_packet[j] ^= collection_pair.second.packets()[i][j];
                                }
                            }

                            auto fec_time = std::chrono::steady_clock::now() - fec_start;

                            printf("restored %d %d %lf\n", missing_frame_id, missing_packet_index, fec_time.count() / 1000000.0f);
                            frame_packet_collections.at(missing_frame_id).addPacket(missing_packet_index, std::move(fec_frame_packet));
                        } // end of for (int missing_packet_index : missing_packet_indices)

                        for (int fec_failed_packet_index : fec_failed_packet_indices) {
                            printf("request %d %d\n", missing_frame_id, fec_failed_packet_index);
                        }
                        receiver.send(missing_frame_id, fec_failed_packet_indices);
                    }
                } // Forward Error Correction End
            }

            frame_packet_collections.at(frame_id).addPacket(packet_index, std::move(frame_packet));
        }

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
            frame_message_queue.enqueue(std::move(frame_packet_collections.at(full_frame_id).toMessage()));
            frame_packet_collections.erase(full_frame_id);
        }

        if(!frame_packet_collections.empty())
            printf("Collection Status:\n");

        for (auto collection_pair : frame_packet_collections) {
            int frame_id = collection_pair.first;
            auto collected_packet_count = collection_pair.second.getCollectedPacketCount();
            auto total_packet_count = collection_pair.second.packet_count();
            printf("collection frame_id: %d, collected: %d, total: %d\n", frame_id,
                   collected_packet_count, total_packet_count);
        }

        // Clean up frame_packet_collections.
        {
            std::vector<int> obsolete_frame_ids;
            for (auto& collection_pair : frame_packet_collections) {
                if (collection_pair.first <= last_frame_id) {
                    obsolete_frame_ids.push_back(collection_pair.first);
                }
            }

            for (int obsolete_frame_id : obsolete_frame_ids) {
                frame_packet_collections.erase(obsolete_frame_id);
            }
        }

        // Clean up xor_packet_collections.
        {
            std::vector<int> obsolete_frame_ids;
            for (auto& collection_pair : xor_packet_collections) {
                if (collection_pair.first <= last_frame_id) {
                    obsolete_frame_ids.push_back(collection_pair.first);
                }
            }

            for (int obsolete_frame_id : obsolete_frame_ids) {
                xor_packet_collections.erase(obsolete_frame_id);
            }
        }
    }
}

void receive_frames(std::string ip_address, int port)
{
    const int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;
    //const int RECEIVER_RECEIVE_BUFFER_SIZE = 64 * 1024;
    asio::io_context io_context;
    Receiver receiver(io_context, RECEIVER_RECEIVE_BUFFER_SIZE);
    receiver.ping(ip_address, port);

    printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

    bool stop_receiver_thread = false;
    moodycamel::ReaderWriterQueue<std::vector<uint8_t>> init_packet_queue;
    moodycamel::ReaderWriterQueue<FrameMessage> frame_message_queue;
    int last_frame_id = -1;
    int summary_packet_count = 0;
    std::thread receiver_thread(run_receiver_thread, std::ref(stop_receiver_thread), std::ref(receiver),
                                std::ref(init_packet_queue), std::ref(frame_message_queue),
                                std::ref(last_frame_id), std::ref(summary_packet_count));

    Vp8Decoder color_decoder;
    int depth_width;
    int depth_height;
    std::unique_ptr<TrvlDecoder> depth_decoder;

    std::vector<FrameMessage> frame_messages;

    auto frame_start = std::chrono::steady_clock::now();
    for (;;) {
        std::vector<uint8_t> packet;
        while (init_packet_queue.try_dequeue(packet)) {
            int cursor = 0;
            //int session_id;
            //memcpy(&session_id, packet.data() + cursor, 4);
            cursor += 4;

            //uint8_t packet_type = packet[cursor];
            cursor += 1;

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
        
        FrameMessage frame_message;
        while (frame_message_queue.try_dequeue(frame_message)) {
            frame_messages.push_back(frame_message);
        }

        std::sort(frame_messages.begin(), frame_messages.end(), [](const FrameMessage& lhs, const FrameMessage& rhs)
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
        std::chrono::steady_clock::duration packet_collection_time;

        auto decoder_start = std::chrono::steady_clock::now();
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

        // Reset frame_messages after they are displayed.
        frame_messages = std::vector<FrameMessage>();
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

        try {
            receive_frames(ip_address, port);
        } catch (std::exception & e) {
            printf("Error from _receive_frames: %s\n", e.what());
        }
    }
}
}

int main()
{
    kh::main();
    return 0;
}