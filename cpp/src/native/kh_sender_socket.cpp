#include "kh_sender_socket.h"

#include <algorithm>
#include <iostream>
#include "kh_packet_helper.h"

namespace kh
{
SenderSocket::SenderSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint,
                     int send_buffer_size)
    : socket_{std::move(socket)}, remote_endpoint_{remote_endpoint}
{
    socket_.non_blocking(true);
    socket_.set_option(asio::socket_base::send_buffer_size{send_buffer_size});
}

// Sends a Kinect calibration information to a Receiver.
void SenderSocket::sendInitPacket(int session_id, k4a_calibration_t calibration, std::error_code& asio_error)
{
    const auto depth_intrinsics{calibration.depth_camera_calibration.intrinsics.parameters.param};
    const int depth_width{calibration.depth_camera_calibration.resolution_width};
    const int depth_height{calibration.depth_camera_calibration.resolution_height};
    const float depth_metric_radius{calibration.depth_camera_calibration.metric_radius};

    const auto color_intrinsics{calibration.color_camera_calibration.intrinsics.parameters.param};
    const int color_width{calibration.color_camera_calibration.resolution_width};
    const int color_height{calibration.color_camera_calibration.resolution_height};
    const float color_metric_radius{calibration.color_camera_calibration.metric_radius};

    const auto depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

    const int packet_size = gsl::narrow_cast<uint32_t>(sizeof(session_id) +
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

    std::vector<std::byte> packet(packet_size);
    int cursor = 0;

    // Message type
    memcpy(packet.data() + cursor, &session_id, sizeof(session_id));
    cursor += sizeof(session_id);

    //packet[cursor] = KH_SENDER_INIT_PACKET;
    memcpy(packet.data() + cursor, &KH_SENDER_INIT_PACKET, 1);
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

    sendPacket(packet, asio_error);
}

void SenderSocket::sendAudioPacket(int session_id, int frame_id, const std::vector<uint8_t>& opus_frame, int opus_frame_size, std::error_code& asio_error)
{
    const int packet_size = gsl::narrow_cast<int>(sizeof(session_id) +
                                                  1 +
                                                  sizeof(frame_id) +
                                                  sizeof(opus_frame_size) +
                                                  opus_frame_size);

    std::vector<std::byte> packet(packet_size);
    int cursor = 0;

    memcpy(packet.data() + cursor, &session_id, sizeof(session_id));
    cursor += sizeof(session_id);

    //packet[cursor] = KH_SENDER_AUDIO_PACKET;
    memcpy(packet.data() + cursor, &KH_SENDER_AUDIO_PACKET, 1);
    cursor += 1;

    memcpy(packet.data() + cursor, &frame_id, sizeof(frame_id));
    cursor += sizeof(frame_id);

    memcpy(packet.data() + cursor, &opus_frame_size, sizeof(opus_frame_size));
    cursor += sizeof(opus_frame_size);

    memcpy(packet.data() + cursor, opus_frame.data(), opus_frame_size);

    printf("packet_size: %ld\n", packet_size);
    sendPacket(packet, asio_error);
}

std::optional<std::vector<std::byte>> SenderSocket::receive(std::error_code& asio_error)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    asio::ip::udp::endpoint sender_endpoint;
    const size_t packet_size = socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, asio_error);

    if (asio_error)
        return std::nullopt;

    packet.resize(packet_size);
    return packet;
}

std::vector<std::byte> SenderSocket::createFrameMessage(float frame_time_stamp, bool keyframe, gsl::span<const std::byte> vp8_frame,
                                                        gsl::span<const std::byte> depth_encoder_frame)
{
    const int message_size{gsl::narrow_cast<int>(4 + 1 + 4 + vp8_frame.size() + 4 + depth_encoder_frame.size())};

    std::vector<std::byte> message(message_size);
    int cursor = 0;

    memcpy(message.data() + cursor, &frame_time_stamp, 4);
    cursor += 4;

    //message[cursor] = static_cast<uint8_t>(keyframe);
    memcpy(message.data() + cursor, &keyframe, 1);
    cursor += 1;

    int vp8_frame_size = vp8_frame.size();
    memcpy(message.data() + cursor, &vp8_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, vp8_frame.data(), vp8_frame.size());
    cursor += vp8_frame.size();

    int depth_encoder_frame_size{gsl::narrow_cast<int>(depth_encoder_frame.size())};
    memcpy(message.data() + cursor, &depth_encoder_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, depth_encoder_frame.data(), depth_encoder_frame_size);

    return message;
}

std::vector<std::vector<std::byte>> SenderSocket::createFramePackets(int session_id, int frame_id, gsl::span<const std::byte> frame_message)
{
    // The size of frame packets is defined to match the upper limit for udp packets.
    int packet_count{gsl::narrow_cast<int>(frame_message.size() - 1) / KH_MAX_VIDEO_PACKET_CONTENT_SIZE + 1};
    std::vector<std::vector<std::byte>> packets;
    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
        int message_cursor = KH_MAX_VIDEO_PACKET_CONTENT_SIZE * packet_index;

        const bool last{(packet_index + 1) == packet_count};
        const int packet_content_size{last ? gsl::narrow_cast<int>(frame_message.size() - message_cursor) : KH_MAX_VIDEO_PACKET_CONTENT_SIZE};

        std::vector<std::byte> packet(KH_PACKET_SIZE);
        int cursor = 0;
        memcpy(packet.data() + cursor, &session_id, 4);
        cursor += 4;

        memcpy(packet.data() + cursor, &KH_SENDER_VIDEO_PACKET, 1);
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
std::vector<std::vector<std::byte>> SenderSocket::createXorPackets(int session_id, int frame_id,
                                                                   gsl::span<const std::vector<std::byte>> frame_packets, int max_group_size)
{
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    const int xor_packet_count{gsl::narrow_cast<int>(frame_packets.size() - 1) / max_group_size + 1};

    std::vector<std::vector<std::byte>> xor_packets;
    for (int xor_packet_index = 0; xor_packet_index < xor_packet_count; ++xor_packet_index) {
        const int begin_index{xor_packet_index * max_group_size};
        const int end_index{std::min<int>(begin_index + max_group_size, frame_packets.size())};
        
        // Copy packets[begin_index] instead of filling in everything zero
        // to reduce an XOR operation for contents once.
        std::vector<std::byte> xor_packet{frame_packets[begin_index]};
        int cursor = 0;
        memcpy(xor_packet.data() + cursor, &session_id, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &KH_SENDER_XOR_PACKET, 1);
        cursor += 1;

        memcpy(xor_packet.data() + cursor, &frame_id, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &xor_packet_index, 4);
        cursor += 4;

        memcpy(xor_packet.data() + cursor, &xor_packet_count, 4);
        //cursor += 4;

        for (int i = begin_index + 1; i < end_index; ++i) {
            for (int j = KH_VIDEO_PACKET_HEADER_SIZE; j < KH_PACKET_SIZE; ++j) {
                xor_packet[j] ^= frame_packets[i][j];
            }
        }
        xor_packets.push_back(std::move(xor_packet));
    }
    return xor_packets;
}

void SenderSocket::sendPacket(const std::vector<std::byte>& packet, std::error_code& error)
{
    socket_.send_to(asio::buffer(packet), remote_endpoint_, 0, error);
}
}