#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <gsl/gsl>
#include <k4a/k4a.h>

namespace kh
{
// Packet types.
const uint8_t KH_SENDER_INIT_PACKET = 0;
const uint8_t KH_SENDER_VIDEO_PACKET = 1;
const uint8_t KH_SENDER_XOR_PACKET = 2;
const uint8_t KH_SENDER_AUDIO_PACKET = 3;

const uint8_t KH_RECEIVER_PING_PACKET = 0;
const uint8_t KH_RECEIVER_REPORT_PACKET = 1;
const uint8_t KH_RECEIVER_REQUEST_PACKET = 2;

const int KH_PACKET_SIZE = 1472;

// Video packets need more information for reassembly of packets.
const int KH_VIDEO_PACKET_HEADER_SIZE = 17;
const int KH_MAX_VIDEO_PACKET_CONTENT_SIZE = KH_PACKET_SIZE - KH_VIDEO_PACKET_HEADER_SIZE;

// Opus packets are small enough to fit in UDP.
const int KH_AUDIO_PACKET_HEADER_SIZE = 13;
const int KH_MAX_AUDIO_PACKET_CONTENT_SIZE = KH_PACKET_SIZE - KH_AUDIO_PACKET_HEADER_SIZE;

struct PacketCursor
{
    int position{0};
};

template<class T>
void copy_to_bytes(const T& t, gsl::span<std::byte> bytes, int& cursor)
{
    memcpy(&bytes[cursor], &t, sizeof(T));
    cursor += sizeof(T);
}

template<class T>
void copy_from_bytes(T& t, gsl::span<const std::byte> bytes, int& cursor)
{
    memcpy(&t, &bytes[cursor], sizeof(T));
    cursor += sizeof(T);
}

template<class T>
T copy_from_bytes(gsl::span<const std::byte> bytes, int& cursor)
{
    T t;
    copy_from_bytes(t, bytes, cursor);
    return t;
}

struct InitSenderPacketData
{
    int color_width;
    int color_height;
    int depth_width;
    int depth_height;
    k4a_calibration_intrinsic_parameters_t::_param color_intrinsics;
    float color_metric_radius;
    k4a_calibration_intrinsic_parameters_t::_param depth_intrinsics;
    float depth_metric_radius;
    k4a_calibration_extrinsics_t depth_to_color_extrinsics;
};

struct FecSenderPacketData
{
    int frame_id;
    int packet_index;
    int packet_count;
    std::vector<std::byte> message_data;
};

int get_session_id_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);
uint8_t get_packet_type_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration);
std::vector<std::byte> create_init_sender_packet_bytes(int session_id, InitSenderPacketData init_sender_packet_data);
InitSenderPacketData parse_init_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

std::vector<std::byte> create_frame_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame);
std::vector<std::vector<std::byte>> split_frame_sender_message_bytes(int session_id, int frame_id,
                                                                     gsl::span<const std::byte> frame_message);
std::vector<std::byte> create_frame_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                        gsl::span<const std::byte> packet_content);
// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_fec_sender_packet_bytes_vector(int session_id, int frame_id, int max_group_size,
                                                                          gsl::span<const std::vector<std::byte>> frame_packet_bytes_span);
std::vector<std::byte> create_fec_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                      gsl::span<const std::vector<std::byte>> frame_packet_bytes_vector);
FecSenderPacketData parse_fec_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

}