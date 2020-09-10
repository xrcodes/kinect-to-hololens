#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>
#include <gsl/gsl>
#include <k4a/k4a.h>

namespace k4a
{
struct calibration;
}

namespace kh
{
// Packet types.
enum class SenderPacketType : std::uint8_t
{
    Confirm = 0,
    Heartbeat = 1,
    Video = 2,
    Parity = 3,
    Audio = 4,
};

enum class ReceiverPacketType : std::uint8_t
{
    Connect = 0,
    Heartbeat = 1,
    Report = 2,
    Request = 3,
};

constexpr int KH_PACKET_SIZE{1472};

// Video packets need more information for reassembly of packets.
constexpr int KH_VIDEO_PACKET_HEADER_SIZE{17};
constexpr int KH_MAX_VIDEO_PACKET_CONTENT_SIZE{KH_PACKET_SIZE - KH_VIDEO_PACKET_HEADER_SIZE};

// Opus packets are small enough to fit in UDP.
constexpr int KH_AUDIO_PACKET_HEADER_SIZE{13};
constexpr int KH_MAX_AUDIO_PACKET_CONTENT_SIZE{KH_PACKET_SIZE - KH_AUDIO_PACKET_HEADER_SIZE};

constexpr static int KH_FEC_PARITY_GROUP_SIZE{2};

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

struct ConfirmSenderPacketData
{
    int receiver_session_id{0};
};

std::vector<std::byte> create_confirm_sender_packet_bytes(int session_id, int receiver_session_id);

std::vector<std::byte> create_heartbeat_sender_packet_bytes(int session_id);

//VideoInitSenderPacketData create_video_init_sender_packet_data(const k4a::calibration& calibration);
//std::vector<std::byte> create_video_init_sender_packet_bytes(int session_id, const VideoInitSenderPacketData& video_init_sender_packet_data);
//VideoInitSenderPacketData parse_video_init_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct VideoSenderMessageData
{
    float frame_time_stamp{0.0f};
    bool keyframe{false};
    int width{0};
    int height{0};
    float cx{0.0f};
    float cy{0.0f};
    float fx{0.0f};
    float fy{0.0f};
    float k1{0.0f};
    float k2{0.0f};
    float k3{0.0f};
    float k4{0.0f};
    float k5{0.0f};
    float k6{0.0f};
    float codx{0.0f};
    float cody{0.0f};
    float p1{0.0f};
    float p2{0.0f};
    float max_radius_for_projection{0.0f};
    std::vector<std::byte> color_encoder_frame;
    std::vector<std::byte> depth_encoder_frame;
    std::optional<std::array<float, 4>> floor;
};

struct VideoSenderPacketData
{
    int frame_id{0};
    int packet_index{0};
    int packet_count{0};
    std::vector<std::byte> message_data;
};

std::vector<std::byte> create_video_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         const k4a::calibration& calibration,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame,
                                                         std::optional<std::array<float, 4>> floor);
std::vector<std::vector<std::byte>> split_video_sender_message_bytes(int session_id, int frame_id,
                                                                     gsl::span<const std::byte> video_message);
std::vector<std::byte> create_video_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                        gsl::span<const std::byte> packet_content);
VideoSenderPacketData parse_video_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);
std::vector<std::byte> merge_video_sender_message_bytes(gsl::span<gsl::span<std::byte>> video_sender_message_data_set);
VideoSenderMessageData parse_video_sender_message_bytes(gsl::span<const std::byte> message_bytes);

struct ParitySenderPacketData
{
    int frame_id{0};
    int packet_index{0};
    int packet_count{0};
    std::vector<std::byte> bytes;
};

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_parity_sender_packet_bytes_set(int session_id, int frame_id, int parity_group_size,
                                                                          gsl::span<const std::vector<std::byte>> frame_packet_bytes_span);
std::vector<std::byte> create_parity_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                         gsl::span<const std::vector<std::byte>> frame_packet_bytes_set);
ParitySenderPacketData parse_parity_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct AudioSenderPacketData
{
    int frame_id{0};
    std::vector<std::byte> opus_frame;
};

std::vector<std::byte> create_audio_sender_packet_bytes(int session_id, int frame_id,
                                                        gsl::span<const std::byte> opus_frame);
AudioSenderPacketData parse_audio_sender_packet_bytes(gsl::span<const std::byte> packet_bytes);

std::vector<std::byte> create_floor_sender_packet_bytes(int session_id, float a, float b, float c, float d);

/**Receiver Packets**/
int get_session_id_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);
ReceiverPacketType get_packet_type_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct ConnectReceiverPacketData
{
    bool video_requested{false};
    bool audio_requested{false};
};

std::vector<std::byte> create_connect_receiver_packet_bytes(int session_id,
                                                            bool video_requested,
                                                            bool audio_requested);
ConnectReceiverPacketData parse_connect_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);

std::vector<std::byte> create_heartbeat_receiver_packet_bytes(int session_id);

struct ReportReceiverPacketData
{
    int frame_id{0};
    float decoder_time_ms{0.0f};
    float frame_time_ms{0.0f};
};

std::vector<std::byte> create_report_receiver_packet_bytes(int session_id, int frame_id, float decoder_time_ms, float frame_time_ms);
ReportReceiverPacketData parse_report_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);

struct RequestReceiverPacketData
{
    int frame_id{0};
    std::vector<int> video_packet_indices;
    std::vector<int> parity_packet_indices;
};

std::vector<std::byte> create_request_receiver_packet_bytes(int session_id, int frame_id,
                                                            const std::vector<int>& video_packet_indices,
                                                            const std::vector<int>& parity_packet_indices);
RequestReceiverPacketData parse_request_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes);
}