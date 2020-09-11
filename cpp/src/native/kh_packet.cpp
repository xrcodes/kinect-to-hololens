#include "kh_packet.h"

#include <iostream>
#include "kh_kinect.h"

namespace kh
{
int get_sender_id_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<int>(packet_bytes, 0);
}

SenderPacketType get_packet_type_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<SenderPacketType>(packet_bytes, sizeof(int));
}

Packet create_confirm_sender_packet(int sender_id, int receiver_id)
{
    constexpr auto packet_size{sizeof(sender_id) +
                               sizeof(SenderPacketType) +
                               sizeof(receiver_id)};

    Packet packet{packet_size};
    PacketCursor cursor;
    copy_to_packet(sender_id, packet, cursor);
    copy_to_packet(SenderPacketType::Confirm, packet, cursor);
    copy_to_packet(receiver_id, packet, cursor);

    return packet;
}

Packet create_heartbeat_sender_packet(int sender_id)
{
    constexpr auto packet_size{sizeof(sender_id) +
                               sizeof(SenderPacketType)};

    Packet packet(packet_size);
    PacketCursor cursor;
    copy_to_packet(sender_id, packet, cursor);
    copy_to_packet(SenderPacketType::Heartbeat, packet, cursor);

    return packet;
}

Message create_video_sender_message(float frame_time_stamp, bool keyframe,
                                    const k4a::calibration& calibration,
                                    gsl::span<const std::byte> color_encoder_frame,
                                    gsl::span<const std::byte> depth_encoder_frame,
                                    std::optional<std::array<float, 4>> floor)
{
    const auto message_size{sizeof(frame_time_stamp) +
                            sizeof(keyframe) +
                            sizeof(int) + // width
                            sizeof(int) + // height
                            sizeof(float) + // cx
                            sizeof(float) + // cy
                            sizeof(float) + // fx
                            sizeof(float) + // fy
                            sizeof(float) + // k1
                            sizeof(float) + // k2
                            sizeof(float) + // k3
                            sizeof(float) + // k4
                            sizeof(float) + // k5
                            sizeof(float) + // k6
                            sizeof(float) + // codx
                            sizeof(float) + // cody
                            sizeof(float) + // p1
                            sizeof(float) + // p2
                            sizeof(float) + // max_radius_for_projection
                            sizeof(int) + // size of color_encoder_frame 
                            sizeof(int) + // size of depth_encoder_frame
                            color_encoder_frame.size() +
                            depth_encoder_frame.size() +
                            sizeof(bool) +
                            (floor.has_value() ? (sizeof(float) * 4) : 0)};
    
    Message message{gsl::narrow<std::vector<std::byte>::size_type>(message_size)};
    MessageCursor cursor;

    copy_to_message(frame_time_stamp, message, cursor);
    copy_to_message(keyframe, message, cursor);
    copy_to_message(calibration.depth_camera_calibration.resolution_width, message, cursor); // width
    copy_to_message(calibration.depth_camera_calibration.resolution_height, message, cursor); // height
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.cx, message, cursor); // cx
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.cy, message, cursor); // cy
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.fx, message, cursor); // fx
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.fy, message, cursor); // fy
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k1, message, cursor); // k1
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k2, message, cursor); // k2
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k3, message, cursor); // k3
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k4, message, cursor); // k4
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k5, message, cursor); // k5
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.k6, message, cursor); // k6
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.codx, message, cursor); // codx
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.cody, message, cursor); // cody
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.p1, message, cursor); // p1
    copy_to_message(calibration.depth_camera_calibration.intrinsics.parameters.param.p2, message, cursor); // p2
    copy_to_message(calibration.depth_camera_calibration.metric_radius, message, cursor); // max_radius_for_projection

    copy_to_message(gsl::narrow<int>(color_encoder_frame.size()), message, cursor);
    memcpy(message.bytes.data() + cursor.position, color_encoder_frame.data(), color_encoder_frame.size());
    cursor.position += gsl::narrow<int>(color_encoder_frame.size());

    copy_to_message(gsl::narrow<int>(depth_encoder_frame.size()), message, cursor);
    memcpy(message.bytes.data() + cursor.position, depth_encoder_frame.data(), depth_encoder_frame.size());
    cursor.position += gsl::narrow<int>(depth_encoder_frame.size());

    copy_to_message(floor.has_value(), message, cursor);

    if (floor) {
        copy_to_message(floor->at(0), message, cursor);
        copy_to_message(floor->at(1), message, cursor);
        copy_to_message(floor->at(2), message, cursor);
        copy_to_message(floor->at(3), message, cursor);
    }

    return message;
}

std::vector<Packet> split_video_sender_message_bytes(int sender_id, int frame_id, gsl::span<const std::byte> video_message)
{
    // The size of frame packets is defined to match the upper limit for udp packets.
    int packet_count{gsl::narrow<int>(video_message.size() - 1) / KH_MAX_VIDEO_PACKET_CONTENT_SIZE + 1};
    std::vector<Packet> packets;
    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
        int message_cursor = KH_MAX_VIDEO_PACKET_CONTENT_SIZE * packet_index;

        const bool last{(packet_index + 1) == packet_count};
        const auto packet_content_size{last ? (video_message.size() - message_cursor) : KH_MAX_VIDEO_PACKET_CONTENT_SIZE};
        packets.push_back(create_video_sender_packet(sender_id, frame_id, packet_index, packet_count,
                                                     gsl::span<const std::byte>{video_message.data() + message_cursor, packet_content_size}));
    }

    return packets;
}

Packet create_video_sender_packet(int sender_id, int frame_id, int packet_index, int packet_count, gsl::span<const std::byte> packet_content)
{
    Packet packet{KH_PACKET_SIZE};
    PacketCursor cursor;
    copy_to_packet(sender_id, packet, cursor);
    copy_to_packet(SenderPacketType::Video, packet, cursor);
    copy_to_packet(frame_id, packet, cursor);
    copy_to_packet(packet_index, packet, cursor);
    copy_to_packet(packet_count, packet, cursor);
    memcpy(packet.bytes.data() + cursor.position, packet_content.data(), packet_content.size());

    return packet;
}

VideoSenderPacket read_video_sender_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    VideoSenderPacket video_sender_packet;
    copy_from_bytes(video_sender_packet.sender_id, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet.type, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet.frame_id, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet.packet_index, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet.packet_count, packet_bytes, cursor);

    video_sender_packet.message_data.resize(packet_bytes.size() - cursor.position);
    memcpy(video_sender_packet.message_data.data(),
           &packet_bytes[cursor.position],
           video_sender_packet.message_data.size());

    return video_sender_packet;
}

std::vector<std::byte> merge_video_sender_packets(gsl::span<VideoSenderPacket*> video_packet_ptrs)
{
    size_t message_size{0};
    for (auto& video_packet_ptr : video_packet_ptrs)
        message_size += video_packet_ptr->message_data.size();

    std::vector<std::byte> message_bytes(message_size);
    size_t cursor{0};
    for (auto& video_packet_ptr : video_packet_ptrs) {
        memcpy(message_bytes.data() + cursor, video_packet_ptr->message_data.data(), video_packet_ptr->message_data.size());
        cursor += video_packet_ptr->message_data.size();
    }

    return message_bytes;
}

VideoSenderMessage read_video_sender_message(gsl::span<const std::byte> message_bytes)
{
    PacketCursor cursor{};
    VideoSenderMessage video_sender_message;
    copy_from_bytes(video_sender_message.frame_time_stamp, message_bytes, cursor);
    copy_from_bytes(video_sender_message.keyframe, message_bytes, cursor);
    copy_from_bytes(video_sender_message.width, message_bytes, cursor);
    copy_from_bytes(video_sender_message.height, message_bytes, cursor);
    copy_from_bytes(video_sender_message.cx, message_bytes, cursor);
    copy_from_bytes(video_sender_message.cy, message_bytes, cursor);
    copy_from_bytes(video_sender_message.fx, message_bytes, cursor);
    copy_from_bytes(video_sender_message.fy, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k1, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k2, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k3, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k4, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k5, message_bytes, cursor);
    copy_from_bytes(video_sender_message.k6, message_bytes, cursor);
    copy_from_bytes(video_sender_message.codx, message_bytes, cursor);
    copy_from_bytes(video_sender_message.cody, message_bytes, cursor);
    copy_from_bytes(video_sender_message.p1, message_bytes, cursor);
    copy_from_bytes(video_sender_message.p2, message_bytes, cursor);
    copy_from_bytes(video_sender_message.max_radius_for_projection, message_bytes, cursor);

    // Parsing the bytes of the message into the VP8 and TRVL frames.
    int color_encoder_frame_size{copy_from_bytes<int>(message_bytes, cursor)};
    video_sender_message.color_encoder_frame = std::vector<std::byte>(color_encoder_frame_size);
    memcpy(video_sender_message.color_encoder_frame.data(),
           message_bytes.data() + cursor.position,
           color_encoder_frame_size);
    cursor.position += color_encoder_frame_size;

    int depth_encoder_frame_size{copy_from_bytes<int>(message_bytes, cursor)};
    video_sender_message.depth_encoder_frame = std::vector<std::byte>(depth_encoder_frame_size);
    memcpy(video_sender_message.depth_encoder_frame.data(),
           message_bytes.data() + cursor.position,
           depth_encoder_frame_size);
    cursor.position += depth_encoder_frame_size;

    bool has_floor;
    copy_from_bytes(has_floor, message_bytes, cursor);

    if (has_floor) {
        std::array<float, 4> floor;
        copy_from_bytes(floor[0], message_bytes, cursor);
        copy_from_bytes(floor[1], message_bytes, cursor);
        copy_from_bytes(floor[2], message_bytes, cursor);
        copy_from_bytes(floor[3], message_bytes, cursor);

        video_sender_message.floor = floor;
    } else {
        video_sender_message.floor = std::nullopt;
    }

    return video_sender_message;
}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<Packet> create_parity_sender_packets(int sender_id, int frame_id, gsl::span<const Packet> video_packets)
{
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    const auto parity_packet_count{(video_packets.size() - 1) / KH_FEC_GROUP_SIZE + 1};

    std::vector<Packet> parity_packets;
    for (int parity_packet_index{0}; parity_packet_index < parity_packet_count; ++parity_packet_index) {
        const int frame_packet_bytes_cursor{parity_packet_index * KH_FEC_GROUP_SIZE};
        const size_t parity_frame_packet_count{std::min<size_t>(KH_FEC_GROUP_SIZE, video_packets.size() - frame_packet_bytes_cursor)};
        parity_packets.push_back(create_parity_sender_packet(sender_id, frame_id, parity_packet_index, gsl::narrow<int>(video_packets.size()),
                                                             gsl::span<const Packet>(&video_packets[frame_packet_bytes_cursor], parity_frame_packet_count)));
    }
    return parity_packets;
}

Packet create_parity_sender_packet(int sender_id, int frame_id, int packet_index, int video_packet_count, gsl::span<const Packet> video_packets)
{
    // Copy packets[begin_index] instead of filling in everything zero
    // to reduce an XOR operation for contents once.
    Packet packet{video_packets[0]};
    PacketCursor cursor;
    copy_to_packet(sender_id, packet, cursor);
    copy_to_packet(SenderPacketType::Parity, packet, cursor);
    copy_to_packet(frame_id, packet, cursor);
    copy_to_packet(packet_index, packet, cursor);
    copy_to_packet(video_packet_count, packet, cursor);

    for (auto i{1}; i < video_packets.size(); ++i) {
        for (gsl::index j{KH_VIDEO_PACKET_HEADER_SIZE}; j < KH_PACKET_SIZE; ++j) {
            packet.bytes[j] ^= video_packets[i].bytes[j];
        }
    }

    return packet;
}

ParitySenderPacket read_parity_sender_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    ParitySenderPacket parity_sender_packet;
    copy_from_bytes(parity_sender_packet.sender_id, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet.type, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet.frame_id, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet.packet_index, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet.video_packet_count, packet_bytes, cursor);

    parity_sender_packet.bytes.resize(packet_bytes.size() - cursor.position);
    memcpy(parity_sender_packet.bytes.data(),
           &packet_bytes[cursor.position],
           parity_sender_packet.bytes.size());

    return parity_sender_packet;
}

Packet create_audio_sender_packet(int sender_id, int frame_id, gsl::span<const std::byte> opus_frame)
{
    const auto packet_size{sizeof(sender_id) +
                           sizeof(SenderPacketType) +
                           sizeof(frame_id) +
                           opus_frame.size()};

    Packet packet(packet_size);
    PacketCursor cursor;
    copy_to_packet(sender_id, packet, cursor);
    copy_to_packet(SenderPacketType::Audio, packet, cursor);
    copy_to_packet(frame_id, packet, cursor);

    memcpy(packet.bytes.data() + cursor.position, opus_frame.data(), opus_frame.size());

    return packet;
}

AudioSenderPacket read_audio_sender_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    AudioSenderPacket audio_sender_packet;
    copy_from_bytes(audio_sender_packet.sender_id, packet_bytes, cursor);
    copy_from_bytes(audio_sender_packet.type, packet_bytes, cursor);
    copy_from_bytes(audio_sender_packet.frame_id, packet_bytes, cursor);

    audio_sender_packet.opus_frame.resize(packet_bytes.size() - cursor.position);
    memcpy(audio_sender_packet.opus_frame.data(),
           packet_bytes.data() + cursor.position,
           audio_sender_packet.opus_frame.size());

    return audio_sender_packet;
}

int get_receiver_id_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<int>(packet_bytes, 0);
}

ReceiverPacketType get_packet_type_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<ReceiverPacketType>(packet_bytes, sizeof(int));
}

Packet create_connect_receiver_packet(int receiver_id, bool video_requested, bool audio_requested)
{
    constexpr auto packet_size{sizeof(receiver_id) +
                               sizeof(ReceiverPacketType) +
                               sizeof(video_requested) +
                               sizeof(audio_requested)};

    Packet packet{packet_size};
    PacketCursor cursor;
    copy_to_packet(receiver_id, packet, cursor);
    copy_to_packet(ReceiverPacketType::Connect, packet, cursor);
    copy_to_packet(video_requested, packet, cursor);
    copy_to_packet(audio_requested, packet, cursor);

    return packet;
}

ConnectReceiverPacket read_connect_receiver_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    ConnectReceiverPacket connect_receiver_packet;
    copy_from_bytes(connect_receiver_packet.receiver_id, packet_bytes, cursor);
    copy_from_bytes(connect_receiver_packet.type, packet_bytes, cursor);
    copy_from_bytes(connect_receiver_packet.video_requested, packet_bytes, cursor);
    copy_from_bytes(connect_receiver_packet.audio_requested, packet_bytes, cursor);

    return connect_receiver_packet;
}

Packet create_heartbeat_receiver_packet(int receiver_id)
{
    constexpr auto packet_size{sizeof(receiver_id) +
                               sizeof(ReceiverPacketType)};

    Packet packet{packet_size};
    PacketCursor cursor;
    copy_to_packet(receiver_id, packet, cursor);
    copy_to_packet(ReceiverPacketType::Heartbeat, packet, cursor);

    return packet;
}

Packet create_report_receiver_packet(int receiver_id, int frame_id, float decoder_time_ms, float frame_time_ms)
{
    constexpr auto packet_size{sizeof(receiver_id) +
                               sizeof(ReceiverPacketType) +
                               sizeof(frame_id) +
                               sizeof(decoder_time_ms) +
                               sizeof(frame_time_ms)};

    Packet packet{packet_size};
    PacketCursor cursor;
    copy_to_packet(receiver_id, packet, cursor);
    copy_to_packet(ReceiverPacketType::Report, packet, cursor);
    copy_to_packet(frame_id, packet, cursor);
    copy_to_packet(decoder_time_ms, packet, cursor);
    copy_to_packet(frame_time_ms, packet, cursor);

    return packet;
}

ReportReceiverPacket read_report_receiver_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    ReportReceiverPacket report_receiver_packet;
    copy_from_bytes(report_receiver_packet.receiver_id, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet.type, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet.frame_id, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet.decoder_time_ms, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet.frame_time_ms, packet_bytes, cursor);

    return report_receiver_packet;
}

Packet create_request_receiver_packet(int receiver_id, int frame_id,
                                      const std::vector<int>& video_packet_indices,
                                      const std::vector<int>& parity_packet_indices)
{
    const auto packet_size(sizeof(receiver_id) +
                           sizeof(ReceiverPacketType) +
                           sizeof(frame_id) +
                           sizeof(int) +
                           sizeof(int) +
                           sizeof(int) * video_packet_indices.size() +
                           sizeof(int) * parity_packet_indices.size());

    Packet packet{packet_size};
    PacketCursor cursor;
    copy_to_packet(receiver_id, packet, cursor);
    copy_to_packet(ReceiverPacketType::Request, packet, cursor);
    copy_to_packet(frame_id, packet, cursor);
    copy_to_packet(gsl::narrow<int>(video_packet_indices.size()), packet, cursor);
    copy_to_packet(gsl::narrow<int>(parity_packet_indices.size()), packet, cursor);

    for (int index : video_packet_indices)
        copy_to_packet(index, packet, cursor);

    for (int index : parity_packet_indices)
        copy_to_packet(index, packet, cursor);

    return packet;
}

RequestReceiverPacket read_request_receiver_packet(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    RequestReceiverPacket request_receiver_packet;
    copy_from_bytes(request_receiver_packet.receiver_id, packet_bytes, cursor);
    copy_from_bytes(request_receiver_packet.type, packet_bytes, cursor);
    copy_from_bytes(request_receiver_packet.frame_id, packet_bytes, cursor);
    
    int video_packet_indices_size;
    int parity_packet_indices_size;
    copy_from_bytes(video_packet_indices_size, packet_bytes, cursor);
    copy_from_bytes(parity_packet_indices_size, packet_bytes, cursor);

    std::vector<int> video_packet_indices(video_packet_indices_size);
    for (int i = 0; i < video_packet_indices_size; ++i)
        copy_from_bytes(video_packet_indices[i], packet_bytes, cursor);

    std::vector<int> parity_packet_indices(parity_packet_indices_size);
    for (int i = 0; i < parity_packet_indices_size; ++i)
        copy_from_bytes(parity_packet_indices[i], packet_bytes, cursor);

    request_receiver_packet.video_packet_indices = video_packet_indices;
    request_receiver_packet.parity_packet_indices = parity_packet_indices;

    return request_receiver_packet;
}
}
