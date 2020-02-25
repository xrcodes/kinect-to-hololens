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

    //packet[cursor] = KH_SENDER_INIT_PACKET;
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

std::vector<std::byte> create_fec_sender_packet_bytes(gsl::span<const std::vector<std::byte>> frame_packets,
                                                      int session_id, int frame_id, int packet_index, int packet_count)
{
    // Copy packets[begin_index] instead of filling in everything zero
    // to reduce an XOR operation for contents once.
    std::vector<std::byte> packet{frame_packets[0]};
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

    for (int i = 1; i < frame_packets.size(); ++i) {
        for (int j = KH_VIDEO_PACKET_HEADER_SIZE; j < KH_PACKET_SIZE; ++j) {
            packet[j] ^= frame_packets[i][j];
        }
    }

    return packet;
}
}