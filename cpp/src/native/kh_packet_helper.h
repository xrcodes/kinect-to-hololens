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

template<class T>
void copy_to_packet(const T& t, gsl::span<std::byte> packet, int& cursor)
{
    memcpy(packet.data() + cursor, &t, sizeof(T));
    cursor += sizeof(T);
}

template<class T>
T copy_from_packet(gsl::span<const std::byte> packet, int& cursor)
{
    T t;
    memcpy(&t, packet.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return t;
}

template<class T>
T copy_from_packet_data(std::byte* packet_data)
{
    T t;
    memcpy(&t, packet_data, sizeof(T));
    return t;
}

struct InitSenderPacketData
{
    k4a_calibration_intrinsic_parameters_t::_param depth_intrinsics;
    int depth_width;
    int depth_height;
    float depth_metric_radius;

    k4a_calibration_intrinsic_parameters_t::_param color_intrinsics;
    int color_width;
    int color_height;
    float color_metric_radius;

    k4a_calibration_extrinsics_t depth_to_color_extrinsics;
};

InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration);
std::vector<std::byte> create_init_sender_packet_bytes(int session_id, InitSenderPacketData init_sender_packet_data);
std::vector<std::byte> create_frame_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                        gsl::span<const std::byte> packet_content);
std::vector<std::byte> create_fec_sender_packet_bytes(gsl::span<const std::vector<std::byte>> frame_packets, 
                                                      int session_id, int frame_id, int packet_index, int packet_count);
}