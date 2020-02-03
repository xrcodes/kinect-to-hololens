#include <chrono>
#include <iostream>
#include <optional>
#include <asio.hpp>
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "kh_receiver_udp.h"
#include "helper/opencv_helper.h"

namespace kh
{
class FrameMessage
{
private:
    FrameMessage(std::vector<uint8_t>&& message, int frame_id, float frame_time_stamp,
                 bool keyframe, int color_encoder_frame_size, int depth_encoder_frame_size)
        : message_(std::move(message)), frame_id_(frame_id), frame_time_stamp_(frame_time_stamp),
        keyframe_(keyframe), color_encoder_frame_size_(color_encoder_frame_size),
        depth_encoder_frame_size_(depth_encoder_frame_size)
    {
    }

public:
    static FrameMessage create(std::vector<uint8_t>&& message)
    {
        int cursor = 0;
        int frame_id;
        memcpy(&frame_id, message.data() + cursor, 4);
        cursor += 4;

        float frame_time_stamp;
        memcpy(&frame_time_stamp, message.data() + cursor, 4);
        cursor += 4;

        bool keyframe = message[cursor];
        cursor += 1;

        // Parsing the bytes of the message into the VP8 and RVL frames.
        int color_encoder_frame_size;
        memcpy(&color_encoder_frame_size, message.data() + cursor, 4);
        cursor += 4;

        // Bytes of the color_encoder_frame.
        cursor += color_encoder_frame_size;

        int depth_encoder_frame_size;
        memcpy(&depth_encoder_frame_size, message.data() + cursor, 4);

        return FrameMessage(std::move(message), frame_id, frame_time_stamp,
                            keyframe, color_encoder_frame_size,
                            depth_encoder_frame_size);
    }

    int frame_id() const { return frame_id_; }
    float frame_time_stamp() const { return frame_time_stamp_; }
    bool keyframe() const { return keyframe_; }
    int color_encoder_frame_size() const { return color_encoder_frame_size_; }
    int depth_encoder_frame_size() const { return depth_encoder_frame_size_; }

    std::vector<uint8_t> getColorEncoderFrame()
    {
        int cursor = 4 + 4 + 1 + 4;
        std::vector<uint8_t> color_encoder_frame(color_encoder_frame_size_);
        memcpy(color_encoder_frame.data(), message_.data() + cursor, color_encoder_frame_size_);

        return color_encoder_frame;
    }

    std::vector<uint8_t> getDepthEncoderFrame()
    {
        int cursor = 4 + 4 + 1 + 4 + color_encoder_frame_size_ + 4;
        std::vector<uint8_t> depth_encoder_frame(depth_encoder_frame_size_);
        memcpy(depth_encoder_frame.data(), message_.data() + cursor, depth_encoder_frame_size_);

        return depth_encoder_frame;
    }

private:
    std::vector<uint8_t> message_;
    int frame_id_;
    float frame_time_stamp_;
    bool keyframe_;
    int color_encoder_frame_size_;
    int depth_encoder_frame_size_;
};

class FramePacketCollection
{
public:
    FramePacketCollection(int frame_id, int packet_count)
        : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count_)
    {
    }

    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }

    void addPacket(int packet_index, std::vector<uint8_t> packet)
    {
        packets_[packet_index] = packet;
    }

    bool isFull()
    {
        for (auto packet : packets_) {
            if (packet.empty())
                return false;
        }

        return true;
    }

    FrameMessage toMessage() {
        int message_size = 0;
        for (auto packet : packets_) {
            message_size += packet.size() - 13;
        }

        std::vector<uint8_t> message(message_size);
        for (int i = 0; i < packets_.size(); ++i) {
            int cursor = (1500 - 13) * i;
            memcpy(message.data() + cursor, packets_[i].data() + 13, packets_[i].size() - 13);
        }

        return FrameMessage::create(std::move(message));
    }

    int getCollectedPacketCount() {
        int count = 0;
        for (auto packet : packets_) {
            if (!packet.empty())
                ++count;
        }

        return count;
    }

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::vector<std::uint8_t>> packets_;
};

void _receive_frames(std::string ip_address, int port)
{
    asio::io_context io_context;
    ReceiverUdp receiver(io_context, 1024 * 1024);
    receiver.ping(ip_address, port);

    std::cout << "sent endpoint" << std::endl;

    Vp8Decoder color_decoder;
    int depth_width;
    int depth_height;
    std::unique_ptr<TrvlDecoder> depth_decoder;

    std::unordered_map<int, FramePacketCollection> frame_packet_collections;
    std::vector<FrameMessage> frame_messages;
    int last_frame_id = -1;

    auto previous_render = std::chrono::steady_clock::now();
    int summary_frame_count;
    int summary_keyframe_count;
    for (;;) {
        auto packet_receive_start = std::chrono::steady_clock::now();
        std::vector<std::vector<uint8_t>> packets;
        for (;;) {
            auto receive_result = receiver.receive();
            if (!receive_result) {
                break;
            }

            // Simulate packet loss
            //if (rand() % 200 == 0) {
            //    continue;
            //}

            packets.push_back(*receive_result);
        }

        auto packet_collection_start = std::chrono::steady_clock::now();
        for (auto packet : packets) {
            int cursor = 0;
            uint8_t packet_type = packet[0];
            cursor += 1;
            if (packet_type == 0) {
                std::cout << "Received a message for initialization." << std::endl;

                // for color width
                cursor += 4;
                // for color height
                cursor += 4;

                memcpy(&depth_width, packet.data() + cursor, 4);
                cursor += 4;

                memcpy(&depth_height, packet.data() + cursor, 4);
                cursor += 4;

                depth_decoder = std::make_unique<TrvlDecoder>(depth_width * depth_height);
                continue;
            } else if (packet_type == 1) {
                int frame_id;
                int packet_index;
                int packet_count;
                memcpy(&frame_id, packet.data() + 1, 4);
                memcpy(&packet_index, packet.data() + 5, 4);
                memcpy(&packet_count, packet.data() + 9, 4);

                auto it = frame_packet_collections.find(frame_id);
                if (it == frame_packet_collections.end())
                    frame_packet_collections.insert({ frame_id, FramePacketCollection(frame_id, packet_count) });

                frame_packet_collections.at(frame_id).addPacket(packet_index, packet);
            }
        }

        auto full_frame_start = std::chrono::steady_clock::now();
        // Find all full collections and their frame_ids.
        std::vector<int> full_frame_ids;
        for (auto collection_pair : frame_packet_collections) {
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

        if (frame_messages.empty()) {
            //int total_collected_packet_count = 0;
            //for (auto collection_pair : frame_packet_collections) {
            //    total_collected_packet_count += collection_pair.second.getCollectedPacketCount();
            //}

            //auto total_time = std::chrono::steady_clock::now() - packet_receive_start;
            //printf("frame_messages empty packet: %d, time: %lld\n", total_collected_packet_count, total_time.count() / 1000000);
            continue;
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

        auto decoder_start = std::chrono::steady_clock::now();
        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        for (int i = *begin_index; i < frame_messages.size(); ++i) {
            auto frame_message_ptr = &frame_messages[i];

            int frame_id = frame_message_ptr->frame_id();
            bool keyframe = frame_message_ptr->keyframe();

            last_frame_id = frame_id;

            auto color_encoder_frame = frame_message_ptr->getColorEncoderFrame();
            auto depth_encoder_frame = frame_message_ptr->getDepthEncoderFrame();

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(color_encoder_frame.data(), color_encoder_frame.size());

            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder->decode(depth_encoder_frame.data(), keyframe);

            if (keyframe)
                ++summary_keyframe_count;
            ++summary_frame_count;
        }

        // There will be frames. Otherwise, the program would not have reached the above for loop.
        receiver.send(last_frame_id);

        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame->av_frame()));
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()), depth_width, depth_height);

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Clean up frame_packet_collections.
        int end_frame_id = frame_messages[frame_messages.size() - 1].frame_id();
        std::vector<int> obsolete_frame_ids;
        for (auto collection_pair : frame_packet_collections) {
            if (collection_pair.first <= end_frame_id) {
                obsolete_frame_ids.push_back(collection_pair.first);
            }
        }

        for (int obsolete_frame_id : obsolete_frame_ids) {
            frame_packet_collections.erase(obsolete_frame_id);
        }

        // Reset frame_messages after they are displayed.
        frame_messages = std::vector<FrameMessage>();

        auto receiver_end = std::chrono::steady_clock::now();

        //printf("summary frame_count: %d, keyframe_count: %d\n", summary_frame_count, summary_keyframe_count);

        auto packet_receive_time = packet_collection_start - packet_receive_start;
        auto total_time = receiver_end - packet_receive_start;

        auto since_last_render = std::chrono::steady_clock::now() - previous_render;

        printf("total_time: %lld, since last: %lld\n", total_time.count() / 1000000, since_last_render.count() / 1000000);

        previous_render = std::chrono::steady_clock::now();
    }
}

void receive_frames()
{
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
        int port = port_line.empty() ? 7777 : std::stoi(port_line);
        try {
            _receive_frames(ip_address, port);
        } catch (std::exception & e) {
            std::cout << e.what() << std::endl;
        }
    }
}
}

void main()
{
    kh::receive_frames();
}