#include <iostream>
#include <asio.hpp>
#include <optional>
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "helper/opencv_helper.h"

namespace kh
{
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

    std::vector<uint8_t> toMessage() {
        int message_size = 0;
        for (auto packet : packets_) {
            message_size += packet.size() - 13;
        }

        std::vector<uint8_t> message(message_size);
        for (int i = 0; i < packets_.size(); ++i) {
            int cursor = (1500 - 13) * i;
            memcpy(message.data() + cursor, packets_[i].data() + 13, packets_[i].size() - 13);
        }

        return message;
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
    asio::ip::udp::endpoint receiver_endpoint(asio::ip::address::from_string(ip_address), port);

    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());
    socket.non_blocking(true);

    // Need a large enough receive buffer, or packets get lost.
    asio::socket_base::receive_buffer_size option(1024 * 1024 - 1);
    socket.set_option(option);

    std::array<char, 1> send_buf = { { 0 } };
    socket.send_to(asio::buffer(send_buf), receiver_endpoint);

    std::cout << "sent endpoint" << std::endl;

    Vp8Decoder color_decoder;
    int depth_width = 640;
    int depth_height = 576;
    std::unique_ptr<TrvlDecoder> depth_decoder = std::make_unique<TrvlDecoder>(depth_width * depth_height);

    int last_frame_id = -1;
    std::unordered_map<int, std::vector<uint8_t>> frame_messages;
    std::unordered_map<int, FramePacketCollection> frame_packet_collections;

    for (;;) {
        std::vector<std::vector<uint8_t>> packets;

        for (;;) {
            std::vector<uint8_t> packet(1500);
            asio::ip::udp::endpoint sender_endpoint;
            std::error_code error;
            size_t packet_size = socket.receive_from(asio::buffer(packet), sender_endpoint, 0, error);

            if (error == asio::error::would_block) {
                break;
            } else if (error) {
                std::cout << "error: " << error.message() << std::endl;
            }

            packet.resize(packet_size);
            packets.push_back(packet);
        }

        for (auto packet : packets) {
            uint8_t packet_type = packet[0];
            int frame_id;
            int packet_index;
            int packet_count;
            memcpy(&frame_id, packet.data() + 1, 4);
            memcpy(&packet_index, packet.data() + 5, 4);
            memcpy(&packet_count, packet.data() + 9, 4);

            auto it = frame_packet_collections.find(frame_id);
            if (it == frame_packet_collections.end()) {
                std::cout << "add collection " << frame_id << std::endl;
                frame_packet_collections.insert({ frame_id, FramePacketCollection(frame_id, packet_count) });
            }

            //frame_packet_collections[frame_id].addPacket(packet_index, packet);
            frame_packet_collections.at(frame_id).addPacket(packet_index, packet);
        }

        // Find all full collections and their frame_ids.
        std::vector<int> full_frame_ids;
        for (auto collection_pair : frame_packet_collections) {
            if (collection_pair.second.isFull()) {
                int frame_id = collection_pair.first;
                full_frame_ids.push_back(frame_id);
                std::cout << "full: " << frame_id << std::endl;
            } else {
                std::cout << "not full1: " << collection_pair.first << std::endl;
                std::cout << "not full2: " << collection_pair.second.packet_count() << std::endl;
                std::cout << "not full3: " << collection_pair.second.getCollectedPacketCount() << std::endl;
            }
        }
        std::sort(full_frame_ids.begin(), full_frame_ids.end());

        // Extract messages from the full collections.
        for (int full_frame_id : full_frame_ids) {
            frame_messages.insert({ full_frame_id, frame_packet_collections.at(full_frame_id).toMessage() });

            frame_packet_collections.erase(full_frame_id);
        }

        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;

        for (;;) {
            auto find_iterator = frame_messages.find(last_frame_id + 1);
            if (find_iterator == frame_messages.end()) {
                break;
            }
            auto frame_message_ptr = &find_iterator->second;

            std::cout << "frame message size: " << frame_message_ptr->size() << std::endl;

            int cursor = 0;
            int message_size;
            memcpy(&message_size, frame_message_ptr->data() + cursor, 4);
            cursor += 4;

            auto message_type = (*frame_message_ptr)[4];
            cursor += 1;

            int frame_id;
            memcpy(&frame_id, frame_message_ptr->data() + cursor, 4);
            cursor += 4;

            std::cout << "frame_id: " << frame_id << std::endl;

            if (frame_id % 100 == 0)
                std::cout << "Received frame " << frame_id << "." << std::endl;
            last_frame_id = frame_id;

            float frame_time_stamp;
            memcpy(&frame_time_stamp, frame_message_ptr->data() + cursor, 4);
            cursor += 4;

            // Parsing the bytes of the message into the VP8 and RVL frames.
            int vp8_frame_size;
            memcpy(&vp8_frame_size, frame_message_ptr->data() + cursor, 4);
            cursor += 4;

            std::vector<uint8_t> vp8_frame(vp8_frame_size);
            memcpy(vp8_frame.data(), frame_message_ptr->data() + cursor, vp8_frame_size);
            cursor += vp8_frame_size;

            int depth_encoder_frame_size;
            memcpy(&depth_encoder_frame_size, frame_message_ptr->data() + cursor, 4);
            cursor += 4;

            std::vector<uint8_t> depth_encoder_frame(depth_encoder_frame_size);
            memcpy(depth_encoder_frame.data(), frame_message_ptr->data() + cursor, depth_encoder_frame_size);
            cursor += depth_encoder_frame_size;

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(vp8_frame.data(), vp8_frame.size());

            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder->decode(depth_encoder_frame.data());
        }

        // If there was a frame meesage
        if (ffmpeg_frame) {
            std::cout << "last_frame_id: " << last_frame_id << std::endl;
            //receiver.send(*last_frame_id);

            std::array<char, 5> frame_id_buffer;
            frame_id_buffer[0] = 1;
            memcpy(frame_id_buffer.data() + 1, &last_frame_id, 4);
            socket.send_to(asio::buffer(frame_id_buffer), receiver_endpoint);

            auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame->av_frame()));
            auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()), depth_width, depth_height);

            // Rendering the depth pixels.
            cv::imshow("Color", color_mat);
            cv::imshow("Depth", depth_mat);
            if (cv::waitKey(1) >= 0)
                break;
        }
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