#include "kh_sender.h"

#include <iostream>

namespace kh
{
Sender::Sender(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint,
                     int send_buffer_size)
    : socket_(std::move(socket)), remote_endpoint_(remote_endpoint)
{
    socket_.non_blocking(true);
    asio::socket_base::send_buffer_size option(send_buffer_size);
    socket_.set_option(option);
}

// Sends a Kinect calibration information to a Receiver.
void Sender::sendInitPacket(int session_id, k4a_calibration_t calibration)
{
    auto depth_intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
    int depth_width = calibration.depth_camera_calibration.resolution_width;
    int depth_height = calibration.depth_camera_calibration.resolution_height;
    float depth_metric_radius = calibration.depth_camera_calibration.metric_radius;

    auto color_intrinsics = calibration.color_camera_calibration.intrinsics.parameters.param;
    int color_width = calibration.color_camera_calibration.resolution_width;
    int color_height = calibration.color_camera_calibration.resolution_height;
    float color_metric_radius = calibration.color_camera_calibration.metric_radius;

    auto depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

    uint32_t packet_size = static_cast<uint32_t>(sizeof(session_id) +
                                                 1 +
                                                 sizeof(color_width) +
                                                 sizeof(color_height) +
                                                 sizeof(depth_width) +
                                                 sizeof(depth_height) +
                                                 sizeof(color_intrinsics) +
                                                 sizeof(color_metric_radius) +
                                                 sizeof(depth_intrinsics) +
                                                 sizeof(depth_metric_radius) +
                                                 sizeof(depth_to_color_extrinsics));

    std::vector<uint8_t> packet(packet_size);
    size_t cursor = 0;

    // Message type
    memcpy(packet.data() + cursor, &session_id, sizeof(color_width));
    cursor += sizeof(session_id);

    packet[cursor] = static_cast<uint8_t>(0);
    cursor += 1;

    memcpy(packet.data() + cursor, &color_width, sizeof(color_width));
    cursor += sizeof(color_width);

    memcpy(packet.data() + cursor, &color_height, sizeof(color_height));
    cursor += sizeof(color_height);

    memcpy(packet.data() + cursor, &depth_width, sizeof(depth_width));
    cursor += sizeof(depth_width);

    memcpy(packet.data() + cursor, &depth_height, sizeof(depth_height));
    cursor += sizeof(depth_height);

    memcpy(packet.data() + cursor, &color_intrinsics, sizeof(color_intrinsics));
    cursor += sizeof(color_intrinsics);

    memcpy(packet.data() + cursor, &color_metric_radius, sizeof(color_metric_radius));
    cursor += sizeof(color_metric_radius);

    memcpy(packet.data() + cursor, &depth_intrinsics, sizeof(depth_intrinsics));
    cursor += sizeof(depth_intrinsics);

    memcpy(packet.data() + cursor, &depth_metric_radius, sizeof(depth_metric_radius));
    cursor += sizeof(depth_metric_radius);

    memcpy(packet.data() + cursor, &depth_to_color_extrinsics, sizeof(depth_to_color_extrinsics));

    sendPacket(packet);
}

std::optional<std::vector<uint8_t>> Sender::receive()
{
    std::vector<uint8_t> packet(1500);
    asio::ip::udp::endpoint sender_endpoint;
    std::error_code error;
    size_t packet_size = socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, error);

    if (error == asio::error::would_block) {
        return std::nullopt;
    }

    if (error) {
        printf("Error from Sender::receive(): %s\n", error.message().c_str());
        throw std::system_error(error);
    }

    packet.resize(packet_size);
    return packet;
}

std::vector<uint8_t> Sender::createFrameMessage(float frame_time_stamp, bool keyframe, std::vector<uint8_t>& vp8_frame,
                                        uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size)
{
    uint32_t message_size = static_cast<uint32_t>(4 + 1 + 4 + vp8_frame.size() + 4 + depth_encoder_frame_size);

    std::vector<uint8_t> message(message_size);
    size_t cursor = 0;

    memcpy(message.data() + cursor, &frame_time_stamp, 4);
    cursor += 4;

    message[cursor] = static_cast<uint8_t>(keyframe);
    cursor += 1;

    int vp8_frame_size = vp8_frame.size();
    memcpy(message.data() + cursor, &vp8_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, vp8_frame.data(), vp8_frame.size());
    cursor += vp8_frame.size();

    memcpy(message.data() + cursor, &depth_encoder_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, depth_encoder_frame, depth_encoder_frame_size);

    return message;
}

std::vector<std::vector<uint8_t>> Sender::createFramePackets(int session_id, int frame_id, const std::vector<uint8_t>& frame_message)
{
    // The size of frame packets is defined to match the upper limit for udp packets.
    const int PACKET_SIZE = 1500;
    const int PACKET_HEADER_SIZE = 17;
    const int MAX_PACKET_CONTENT_SIZE = PACKET_SIZE - PACKET_HEADER_SIZE;

    int packet_count = (frame_message.size() - 1) / MAX_PACKET_CONTENT_SIZE + 1;
    std::vector<std::vector<uint8_t>> packets;
    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
        int message_cursor = MAX_PACKET_CONTENT_SIZE * packet_index;

        bool last = (packet_index + 1) == packet_count;
        int packet_content_size = last ? (frame_message.size() - message_cursor) : MAX_PACKET_CONTENT_SIZE;

        std::vector<uint8_t> packet(PACKET_SIZE);
        uint8_t packet_type = 1;
        int cursor = 0;
        memcpy(packet.data() + cursor, &session_id, 4);
        cursor += 4;

        memcpy(packet.data() + cursor, &packet_type, 1);
        cursor += 1;

        memcpy(packet.data() + cursor, &frame_id, 4);
        cursor += 4;

        memcpy(packet.data() + cursor, &packet_index, 4);
        cursor += 4;

        memcpy(packet.data() + cursor, &packet_count, 4);
        cursor += 4;

        memcpy(packet.data() + cursor, frame_message.data() + message_cursor, packet_content_size);
        // For the last packet, there will be meaningless
        // (MAX_PACKET_CONTENT_SIZE - packet_content_size) bits after the content.

        packets.push_back(std::move(packet));
    }

    return packets;
}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<uint8_t>> Sender::createXorPackets(int session_id, int frame_id,
                                                           const std::vector<std::vector<uint8_t>>& frame_packets, int max_group_size)
{
    const int PACKET_SIZE = 1500;
    const int PACKET_HEADER_SIZE = 17;
    
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    int xor_packet_count = (frame_packets.size() - 1) / max_group_size + 1;

    std::vector<std::vector<uint8_t>> xor_packets;
    for (int xor_packet_index = 0; xor_packet_index < xor_packet_count; ++xor_packet_index) {
        int begin_index = xor_packet_index * max_group_size;
        int end_index = (xor_packet_index + 1) * max_group_size - 1;
        end_index = end_index >= frame_packets.size() ? frame_packets.size() - 1 : end_index;

        // Copy packets[begin_index] instead of filling in everything zero
        // to reduce an XOR operation for contents once.
        std::vector<uint8_t> xor_packet(frame_packets[begin_index]);
        uint8_t packet_type = 2;
        int cursor = 0;
        memcpy(xor_packet.data() + cursor, &session_id, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &packet_type, 1);
        cursor += 1;

        memcpy(xor_packet.data() + cursor, &frame_id, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &xor_packet_index, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &xor_packet_count, 4);
        //cursor += 4;

        for (int j = begin_index + 1; j < begin_index; ++j) {
            for (int k = PACKET_HEADER_SIZE; k < PACKET_SIZE; ++k) {
                xor_packet[k] ^= frame_packets[j][k];
            }
        }
        xor_packets.push_back(std::move(xor_packet));
    }
    return xor_packets;
}

void Sender::sendPacket(const std::vector<uint8_t>& packet)
{
    std::error_code error;
    socket_.send_to(asio::buffer(packet), remote_endpoint_, 0, error);
    if (error) {
        printf("Error from Sender::send(): %s\n", error.message().c_str());
        throw std::system_error(error);
    }
}
}