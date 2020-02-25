#include "kh_sender_socket.h"

#include <algorithm>
#include <iostream>
#include "kh_packets.h"

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
    sendPacket(create_init_sender_packet_bytes(session_id, create_init_sender_packet_data(calibration)), asio_error);
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

//std::vector<std::byte> SenderSocket::createFrameMessage(float frame_time_stamp, bool keyframe, gsl::span<const std::byte> vp8_frame,
//                                                        gsl::span<const std::byte> depth_encoder_frame)
//{
//    const int message_size{gsl::narrow_cast<int>(4 + 1 + 4 + vp8_frame.size() + 4 + depth_encoder_frame.size())};
//
//    std::vector<std::byte> message(message_size);
//    int cursor = 0;
//
//    memcpy(message.data() + cursor, &frame_time_stamp, 4);
//    cursor += 4;
//
//    //message[cursor] = static_cast<uint8_t>(keyframe);
//    memcpy(message.data() + cursor, &keyframe, 1);
//    cursor += 1;
//
//    int vp8_frame_size = vp8_frame.size();
//    memcpy(message.data() + cursor, &vp8_frame_size, 4);
//    cursor += 4;
//
//    memcpy(message.data() + cursor, vp8_frame.data(), vp8_frame.size());
//    cursor += vp8_frame.size();
//
//    int depth_encoder_frame_size{gsl::narrow_cast<int>(depth_encoder_frame.size())};
//    memcpy(message.data() + cursor, &depth_encoder_frame_size, 4);
//    cursor += 4;
//
//    memcpy(message.data() + cursor, depth_encoder_frame.data(), depth_encoder_frame_size);
//
//    return message;
//}

//std::vector<std::vector<std::byte>> SenderSocket::createFramePackets(int session_id, int frame_id, gsl::span<const std::byte> frame_message)
//{
//    // The size of frame packets is defined to match the upper limit for udp packets.
//    int packet_count{gsl::narrow_cast<int>(frame_message.size() - 1) / KH_MAX_VIDEO_PACKET_CONTENT_SIZE + 1};
//    std::vector<std::vector<std::byte>> packets;
//    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
//        int message_cursor = KH_MAX_VIDEO_PACKET_CONTENT_SIZE * packet_index;
//
//        const bool last{(packet_index + 1) == packet_count};
//        const int packet_content_size{last ? gsl::narrow_cast<int>(frame_message.size() - message_cursor) : KH_MAX_VIDEO_PACKET_CONTENT_SIZE};
//        packets.push_back(create_frame_sender_packet_bytes(session_id, frame_id, packet_index, packet_count,
//                                                           gsl::span<const std::byte>{frame_message.data() + message_cursor, packet_content_size}));
//    }
//
//    return packets;
//}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
//std::vector<std::vector<std::byte>> SenderSocket::createXorPackets(int session_id, int frame_id,
//                                                                   gsl::span<const std::vector<std::byte>> frame_packets, int max_group_size)
//{
//    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
//    const int xor_packet_count{gsl::narrow_cast<int>(frame_packets.size() - 1) / max_group_size + 1};
//
//    std::vector<std::vector<std::byte>> xor_packets;
//    for (int xor_packet_index = 0; xor_packet_index < xor_packet_count; ++xor_packet_index) {
//        const int begin_index{xor_packet_index * max_group_size};
//        const int end_index{std::min<int>(max_group_size, frame_packets.size() - begin_index)};
//        xor_packets.push_back(create_fec_sender_packet_bytes(session_id, frame_id, xor_packet_index, xor_packet_count,
//                                                             gsl::span<const std::vector<std::byte>>(&frame_packets[begin_index], end_index)));
//    }
//    return xor_packets;
//}

void SenderSocket::sendPacket(const std::vector<std::byte>& packet, std::error_code& error)
{
    socket_.send_to(asio::buffer(packet), remote_endpoint_, 0, error);
}
}