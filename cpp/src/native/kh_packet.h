#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <gsl/gsl>
#include <k4a/k4a.h>

namespace kh
{
// Packet types.
enum class SenderPacketType : std::uint8_t
{
    Init = 0,
    Video = 1,
    Fec = 2,
    Audio = 3,
};

enum class ReceiverPacketType : std::uint8_t
{
    Ping = 0,
    Report = 1,
    Request = 2,
};

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
void copy_to_bytes(const T& t, gsl::span<std::byte> bytes, PacketCursor& cursor)
{
    memcpy(&bytes[cursor.position], &t, sizeof(T));
    cursor.position += sizeof(T);
}

template<class T>
void copy_from_bytes(T& t, gsl::span<const std::byte> bytes, PacketCursor& cursor)
{
    memcpy(&t, &bytes[cursor.position], sizeof(T));
    cursor.position += sizeof(T);
}

template<class T>
T copy_from_bytes(gsl::span<const std::byte> bytes, PacketCursor& cursor)
{
    T t;
    copy_from_bytes(t, bytes, cursor);
    return t;
}

template<class T>
void copy_from_bytes(T& t, gsl::span<const std::byte> bytes, int position)
{
    memcpy(&t, &bytes[position], sizeof(T));
}

template<class T>
T copy_from_bytes(gsl::span<const std::byte> bytes, int position)
{
    T t;
    copy_from_bytes(t, bytes, position);
    return t;
}

/**Sender Packets**/
int get_session_id_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);
SenderPacketType get_packet_type_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

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

InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration);
std::vector<std::byte> create_init_sender_packet_bytes(int session_id, const InitSenderPacketData& init_sender_packet_data);
InitSenderPacketData parse_init_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct VideoSenderMessageData
{
    float frame_time_stamp;
    bool keyframe;
    std::vector<std::byte> color_encoder_frame;
    std::vector<std::byte> depth_encoder_frame;
};

struct VideoSenderPacketData
{
    int frame_id;
    int packet_index;
    int packet_count;
    std::vector<std::byte> message_data;
};

std::vector<std::byte> create_video_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame);
std::vector<std::vector<std::byte>> split_video_sender_message_bytes(int session_id, int frame_id,
                                                                     gsl::span<const std::byte> video_message);
std::vector<std::byte> create_video_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                        gsl::span<const std::byte> packet_content);
VideoSenderPacketData parse_video_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);
std::vector<std::byte> merge_video_sender_message_bytes(gsl::span<gsl::span<std::byte>> video_sender_message_data_set);
VideoSenderMessageData parse_video_sender_message_bytes(gsl::span<const std::byte> message_bytes);

struct FecSenderPacketData
{
    int frame_id;
    int packet_index;
    int packet_count;
    std::vector<std::byte> bytes;
};

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_fec_sender_packet_bytes_vector(int session_id, int frame_id, int max_group_size,
                                                                          gsl::span<const std::vector<std::byte>> frame_packet_bytes_span);
std::vector<std::byte> create_fec_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                      gsl::span<const std::vector<std::byte>> frame_packet_bytes_vector);
FecSenderPacketData parse_fec_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct AudioSenderPacketData
{
    int frame_id;
    std::vector<std::byte> opus_frame;
};

std::vector<std::byte> create_audio_sender_packet_bytes(int session_id, int frame_id,
                                                        gsl::span<const std::byte> opus_frame);
AudioSenderPacketData parse_audio_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

/**Receiver Packets**/
ReceiverPacketType get_packet_type_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct ReportReceiverPacketData
{
    int frame_id;
    float decoder_time_ms;
    float frame_time_ms;
    int packet_count;
};

ReportReceiverPacketData create_report_receiver_packet_data(int frame_id, float decoder_time_ms,
                                                            float frame_time_ms, int packet_count);
std::vector<std::byte> create_report_receiver_packet_bytes(const ReportReceiverPacketData& report_receiver_packet_data);
ReportReceiverPacketData parse_report_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct RequestReceiverPacketData
{
    int frame_id;
    std::vector<int> packet_indices;
};

RequestReceiverPacketData create_request_receiver_packet_data(int frame_id, const std::vector<int>& packet_indices);
std::vector<std::byte> create_request_receiver_packet_bytes(const RequestReceiverPacketData& request_receiver_packet_data);
RequestReceiverPacketData parse_request_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);
}