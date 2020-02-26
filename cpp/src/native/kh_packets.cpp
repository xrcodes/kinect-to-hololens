#include "kh_packets.h"

namespace kh
{
int get_session_id_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor;
    return copy_from_bytes<int>(packet_bytes, cursor);
}

uint8_t get_packet_type_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{4};
    return copy_from_bytes<uint8_t>(packet_bytes, cursor);
}

InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration)
{
    InitSenderPacketData init_sender_packet_data;
    init_sender_packet_data.color_width = calibration.color_camera_calibration.resolution_width;
    init_sender_packet_data.color_height = calibration.color_camera_calibration.resolution_height;
    init_sender_packet_data.depth_width = calibration.depth_camera_calibration.resolution_width;
    init_sender_packet_data.depth_height = calibration.depth_camera_calibration.resolution_height;
    init_sender_packet_data.color_intrinsics = calibration.color_camera_calibration.intrinsics.parameters.param;
    init_sender_packet_data.color_metric_radius = calibration.color_camera_calibration.metric_radius;
    init_sender_packet_data.depth_intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
    init_sender_packet_data.depth_metric_radius = calibration.depth_camera_calibration.metric_radius;
    init_sender_packet_data.depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

    return init_sender_packet_data;
}

std::vector<std::byte> create_init_sender_packet_bytes(int session_id, InitSenderPacketData init_sender_packet_data)
{
    const int packet_size{gsl::narrow_cast<uint32_t>(sizeof(session_id) + 1 + sizeof(init_sender_packet_data))};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(KH_SENDER_INIT_PACKET, packet_bytes, cursor);
    copy_to_bytes(init_sender_packet_data, packet_bytes, cursor);

    return packet_bytes;
}

InitSenderPacketData parse_init_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{5};
    return copy_from_bytes<InitSenderPacketData>(packet_bytes, cursor);
}

std::vector<std::byte> create_video_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame)
{
    const int message_size{gsl::narrow_cast<int>(4 + 1 + 4 + color_encoder_frame.size() + 4 + depth_encoder_frame.size())};

    std::vector<std::byte> message(message_size);
    PacketCursor cursor;

    copy_to_bytes(frame_time_stamp, message, cursor);
    copy_to_bytes(keyframe, message, cursor);
    copy_to_bytes(gsl::narrow_cast<int>(color_encoder_frame.size()), message, cursor);

    memcpy(message.data() + cursor.position, color_encoder_frame.data(), color_encoder_frame.size());
    cursor.position += color_encoder_frame.size();

    copy_to_bytes(gsl::narrow_cast<int>(depth_encoder_frame.size()), message, cursor);

    memcpy(message.data() + cursor.position, depth_encoder_frame.data(), depth_encoder_frame.size());

    return message;
}

std::vector<std::vector<std::byte>> split_video_sender_message_bytes(int session_id, int frame_id,
                                                                     gsl::span<const std::byte> video_message)
{
    // The size of frame packets is defined to match the upper limit for udp packets.
    int packet_count{gsl::narrow_cast<int>(video_message.size() - 1) / KH_MAX_VIDEO_PACKET_CONTENT_SIZE + 1};
    std::vector<std::vector<std::byte>> packets;
    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
        int message_cursor = KH_MAX_VIDEO_PACKET_CONTENT_SIZE * packet_index;

        const bool last{(packet_index + 1) == packet_count};
        const int packet_content_size{last ? gsl::narrow_cast<int>(video_message.size() - message_cursor) : KH_MAX_VIDEO_PACKET_CONTENT_SIZE};
        packets.push_back(create_video_sender_packet_bytes(session_id, frame_id, packet_index, packet_count,
                                                           gsl::span<const std::byte>{video_message.data() + message_cursor, packet_content_size}));
    }

    return packets;
}

std::vector<std::byte> create_video_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                        gsl::span<const std::byte> packet_content)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet, cursor);
    copy_to_bytes(KH_SENDER_VIDEO_PACKET, packet, cursor);
    copy_to_bytes(frame_id, packet, cursor);
    copy_to_bytes(packet_index, packet, cursor);
    copy_to_bytes(packet_count, packet, cursor);
    memcpy(packet.data() + cursor.position, packet_content.data(), packet_content.size());

    return packet;
}

VideoSenderPacketData parse_video_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{5};
    VideoSenderPacketData video_sender_packet_data;
    copy_from_bytes(video_sender_packet_data.frame_id, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet_data.packet_index, packet_bytes, cursor);
    copy_from_bytes(video_sender_packet_data.packet_count, packet_bytes, cursor);

    video_sender_packet_data.message_data.resize(packet_bytes.size() - cursor.position);
    memcpy(video_sender_packet_data.message_data.data(),
           &packet_bytes[cursor.position],
           video_sender_packet_data.message_data.size());

    return video_sender_packet_data;
}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_fec_sender_packet_bytes_vector(int session_id, int frame_id, int max_group_size,
                                                                          gsl::span<const std::vector<std::byte>> video_packet_bytes_span)
{
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    const int fec_packet_count{gsl::narrow_cast<int>(video_packet_bytes_span.size() - 1) / max_group_size + 1};

    std::vector<std::vector<std::byte>> fec_packets;
    for (gsl::index fec_packet_index{0}; fec_packet_index < fec_packet_count; ++fec_packet_index) {
        const int frame_packet_bytes_cursor{gsl::narrow<int>(fec_packet_index * max_group_size)};
        const int fec_frame_packet_count{std::min<int>(max_group_size, video_packet_bytes_span.size() - frame_packet_bytes_cursor)};
        fec_packets.push_back(create_fec_sender_packet_bytes(session_id, frame_id, fec_packet_index, fec_packet_count,
                                                             gsl::span<const std::vector<std::byte>>(&video_packet_bytes_span[frame_packet_bytes_cursor],
                                                                                                     fec_frame_packet_count)));
    }
    return fec_packets;
}

std::vector<std::byte> create_fec_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                      gsl::span<const std::vector<std::byte>> video_packet_bytes_vector)
{
    // Copy packets[begin_index] instead of filling in everything zero
    // to reduce an XOR operation for contents once.
    std::vector<std::byte> packet{video_packet_bytes_vector[0]};
    PacketCursor cursor;
    copy_to_bytes(session_id, packet, cursor);
    copy_to_bytes(KH_SENDER_XOR_PACKET, packet, cursor);
    copy_to_bytes(frame_id, packet, cursor);
    copy_to_bytes(packet_index, packet, cursor);
    copy_to_bytes(packet_count, packet, cursor);

    for (gsl::index i{1}; i < video_packet_bytes_vector.size(); ++i) {
        for (gsl::index j{KH_VIDEO_PACKET_HEADER_SIZE}; j < KH_PACKET_SIZE; ++j) {
            packet[j] ^= video_packet_bytes_vector[i][j];
        }
    }

    return packet;
}

FecSenderPacketData parse_fec_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{5};
    FecSenderPacketData fec_sender_packet_data;
    copy_from_bytes(fec_sender_packet_data.frame_id, packet_bytes, cursor);
    copy_from_bytes(fec_sender_packet_data.packet_index, packet_bytes, cursor);
    copy_from_bytes(fec_sender_packet_data.packet_count, packet_bytes, cursor);

    fec_sender_packet_data.bytes.resize(packet_bytes.size() - cursor.position);
    memcpy(fec_sender_packet_data.bytes.data(),
           &packet_bytes[cursor.position],
           fec_sender_packet_data.bytes.size());

    return fec_sender_packet_data;
}
}