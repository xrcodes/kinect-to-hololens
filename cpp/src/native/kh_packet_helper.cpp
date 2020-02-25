#include "kh_packet_helper.h"

namespace kh
{
InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration)
{
    InitSenderPacketData init_sender_packet_data;
    init_sender_packet_data.depth_intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
    init_sender_packet_data.depth_width = calibration.depth_camera_calibration.resolution_width;
    init_sender_packet_data.depth_height = calibration.depth_camera_calibration.resolution_height;
    init_sender_packet_data.depth_metric_radius = calibration.depth_camera_calibration.metric_radius;

    init_sender_packet_data.color_intrinsics = calibration.color_camera_calibration.intrinsics.parameters.param;
    init_sender_packet_data.color_width = calibration.color_camera_calibration.resolution_width;
    init_sender_packet_data.color_height = calibration.color_camera_calibration.resolution_height;
    init_sender_packet_data.color_metric_radius = calibration.color_camera_calibration.metric_radius;

    init_sender_packet_data.depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

    return init_sender_packet_data;
}

std::vector<std::byte> create_init_sender_packet_bytes(int session_id, InitSenderPacketData init_sender_packet_data)
{
    const int packet_size = gsl::narrow_cast<uint32_t>(sizeof(session_id) +
                                                       1 +
                                                       sizeof(init_sender_packet_data.color_width) +
                                                       sizeof(init_sender_packet_data.color_height) +
                                                       sizeof(init_sender_packet_data.depth_width) +
                                                       sizeof(init_sender_packet_data.depth_height) +
                                                       sizeof(init_sender_packet_data.color_intrinsics) +
                                                       sizeof(init_sender_packet_data.color_metric_radius) +
                                                       sizeof(init_sender_packet_data.depth_intrinsics) +
                                                       sizeof(init_sender_packet_data.depth_metric_radius) +
                                                       sizeof(init_sender_packet_data.depth_to_color_extrinsics));

    std::vector<std::byte> packet_bytes(packet_size);
    int cursor = 0;

    // Message type
    memcpy(packet_bytes.data() + cursor, &session_id, sizeof(session_id));
    cursor += sizeof(session_id);

    memcpy(packet_bytes.data() + cursor, &KH_SENDER_INIT_PACKET, 1);
    cursor += 1;

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.color_width, sizeof(init_sender_packet_data.color_width));
    cursor += sizeof(init_sender_packet_data.color_width);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.color_height, sizeof(init_sender_packet_data.color_height));
    cursor += sizeof(init_sender_packet_data.color_height);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.depth_width, sizeof(init_sender_packet_data.depth_width));
    cursor += sizeof(init_sender_packet_data.depth_width);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.depth_height, sizeof(init_sender_packet_data.depth_height));
    cursor += sizeof(init_sender_packet_data.depth_height);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.color_intrinsics, sizeof(init_sender_packet_data.color_intrinsics));
    cursor += sizeof(init_sender_packet_data.color_intrinsics);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.color_metric_radius, sizeof(init_sender_packet_data.color_metric_radius));
    cursor += sizeof(init_sender_packet_data.color_metric_radius);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.depth_intrinsics, sizeof(init_sender_packet_data.depth_intrinsics));
    cursor += sizeof(init_sender_packet_data.depth_intrinsics);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.depth_metric_radius, sizeof(init_sender_packet_data.depth_metric_radius));
    cursor += sizeof(init_sender_packet_data.depth_metric_radius);

    memcpy(packet_bytes.data() + cursor, &init_sender_packet_data.depth_to_color_extrinsics, sizeof(init_sender_packet_data.depth_to_color_extrinsics));

    return packet_bytes;
}

std::vector<std::byte> create_frame_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame)
{
    const int message_size{gsl::narrow_cast<int>(4 + 1 + 4 + color_encoder_frame.size() + 4 + depth_encoder_frame.size())};

    std::vector<std::byte> message(message_size);
    int cursor = 0;

    memcpy(message.data() + cursor, &frame_time_stamp, 4);
    cursor += 4;

    memcpy(message.data() + cursor, &keyframe, 1);
    cursor += 1;

    int color_encoder_frame_frame_size = color_encoder_frame.size();
    memcpy(message.data() + cursor, &color_encoder_frame_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, color_encoder_frame.data(), color_encoder_frame.size());
    cursor += color_encoder_frame.size();

    int depth_encoder_frame_size{gsl::narrow_cast<int>(depth_encoder_frame.size())};
    memcpy(message.data() + cursor, &depth_encoder_frame_size, 4);
    cursor += 4;

    memcpy(message.data() + cursor, depth_encoder_frame.data(), depth_encoder_frame_size);

    return message;
}

std::vector<std::vector<std::byte>> split_frame_sender_message_bytes(int session_id, int frame_id, gsl::span<const std::byte> frame_message)
{
    // The size of frame packets is defined to match the upper limit for udp packets.
    int packet_count{gsl::narrow_cast<int>(frame_message.size() - 1) / KH_MAX_VIDEO_PACKET_CONTENT_SIZE + 1};
    std::vector<std::vector<std::byte>> packets;
    for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
        int message_cursor = KH_MAX_VIDEO_PACKET_CONTENT_SIZE * packet_index;

        const bool last{(packet_index + 1) == packet_count};
        const int packet_content_size{last ? gsl::narrow_cast<int>(frame_message.size() - message_cursor) : KH_MAX_VIDEO_PACKET_CONTENT_SIZE};
        packets.push_back(create_frame_sender_packet_bytes(session_id, frame_id, packet_index, packet_count,
                                                           gsl::span<const std::byte>{frame_message.data() + message_cursor, packet_content_size}));
    }

    return packets;
}

std::vector<std::byte> create_frame_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count, gsl::span<const std::byte> packet_content)
{
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

    memcpy(packet.data() + cursor, packet_content.data(), packet_content.size());

    return packet;
}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_fec_sender_packet_bytes_vector(int session_id, int frame_id, int max_group_size,
                                                                          gsl::span<const std::vector<std::byte>> frame_packet_bytes_span)
{
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    const int fec_packet_count{gsl::narrow_cast<int>(frame_packet_bytes_span.size() - 1) / max_group_size + 1};

    std::vector<std::vector<std::byte>> fec_packets;
    for (gsl::index fec_packet_index{0}; fec_packet_index < fec_packet_count; ++fec_packet_index) {
        const int frame_packet_bytes_cursor{gsl::narrow<int>(fec_packet_index * max_group_size)};
        const int fec_frame_packet_count{std::min<int>(max_group_size, frame_packet_bytes_span.size() - frame_packet_bytes_cursor)};
        fec_packets.push_back(create_fec_sender_packet_bytes(session_id, frame_id, fec_packet_index, fec_packet_count,
                                                             gsl::span<const std::vector<std::byte>>(&frame_packet_bytes_span[frame_packet_bytes_cursor],
                                                                                                     fec_frame_packet_count)));
    }
    return fec_packets;
}

std::vector<std::byte> create_fec_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                      gsl::span<const std::vector<std::byte>> frame_packet_bytes_vector)
{
    // Copy packets[begin_index] instead of filling in everything zero
    // to reduce an XOR operation for contents once.
    std::vector<std::byte> packet{frame_packet_bytes_vector[0]};
    int cursor = 0;
    memcpy(packet.data() + cursor, &session_id, 4);
    cursor += 4;

    memcpy(packet.data() + cursor, &KH_SENDER_XOR_PACKET, 1);
    cursor += 1;

    memcpy(packet.data() + cursor, &frame_id, 4);
    cursor += 4;

    memcpy(packet.data() + cursor, &packet_index, 4);
    cursor += 4;

    memcpy(packet.data() + cursor, &packet_count, 4);
    //cursor += 4;

    for (gsl::index i{1}; i < frame_packet_bytes_vector.size(); ++i) {
        for (gsl::index j{KH_VIDEO_PACKET_HEADER_SIZE}; j < KH_PACKET_SIZE; ++j) {
            packet[j] ^= frame_packet_bytes_vector[i][j];
        }
    }

    return packet;
}
}