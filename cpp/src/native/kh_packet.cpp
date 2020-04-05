#include "kh_packet.h"

namespace kh
{
int get_session_id_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<int>(packet_bytes, 0);
}

SenderPacketType get_packet_type_from_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<SenderPacketType>(packet_bytes, 4);
}

InitSenderPacketData create_init_sender_packet_data(k4a_calibration_t calibration)
{
    InitSenderPacketData init_sender_packet_data;
    init_sender_packet_data.width = calibration.depth_camera_calibration.resolution_width;
    init_sender_packet_data.height = calibration.depth_camera_calibration.resolution_height;
    // color_camera_calibration.intrinsics.parameters.param includes metric radius, but actually it is zero.
    // The real metric_radius value for calibration is at color_camera_calibration.metric_radius.
    init_sender_packet_data.intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
    init_sender_packet_data.metric_radius = calibration.depth_camera_calibration.metric_radius;

    return init_sender_packet_data;
}

std::vector<std::byte> create_init_sender_packet_bytes(int session_id, const InitSenderPacketData& init_sender_packet_data)
{
    constexpr int packet_size{gsl::narrow_cast<int>(sizeof(session_id) +
                                                    sizeof(SenderPacketType) +
                                                    sizeof(init_sender_packet_data.width) +
                                                    sizeof(init_sender_packet_data.height) +
                                                    sizeof(init_sender_packet_data.intrinsics) +
                                                    sizeof(init_sender_packet_data.metric_radius))};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(SenderPacketType::Init, packet_bytes, cursor);
    copy_to_bytes(init_sender_packet_data.width, packet_bytes, cursor);
    copy_to_bytes(init_sender_packet_data.height, packet_bytes, cursor);
    copy_to_bytes(init_sender_packet_data.intrinsics, packet_bytes, cursor);
    copy_to_bytes(init_sender_packet_data.metric_radius, packet_bytes, cursor);

    return packet_bytes;
}

InitSenderPacketData parse_init_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    InitSenderPacketData init_sender_packet_data;
    PacketCursor cursor{5};
    copy_from_bytes(init_sender_packet_data.width, packet_bytes, cursor);
    copy_from_bytes(init_sender_packet_data.height, packet_bytes, cursor);
    copy_from_bytes(init_sender_packet_data.intrinsics, packet_bytes, cursor);
    copy_from_bytes(init_sender_packet_data.metric_radius, packet_bytes, cursor);

    return init_sender_packet_data;
}

std::vector<std::byte> create_video_sender_message_bytes(float frame_time_stamp, bool keyframe,
                                                         gsl::span<const std::byte> color_encoder_frame,
                                                         gsl::span<const std::byte> depth_encoder_frame)
{
    const int message_size{gsl::narrow_cast<int>(sizeof(frame_time_stamp) +
                                                 sizeof(keyframe) +
                                                 sizeof(int) +
                                                 color_encoder_frame.size() +
                                                 sizeof(int) +
                                                 depth_encoder_frame.size())};

    std::vector<std::byte> message_bytes(message_size);
    PacketCursor cursor;

    copy_to_bytes(frame_time_stamp, message_bytes, cursor);
    copy_to_bytes(keyframe, message_bytes, cursor);
    copy_to_bytes(gsl::narrow_cast<int>(color_encoder_frame.size()), message_bytes, cursor);
    copy_to_bytes(gsl::narrow_cast<int>(depth_encoder_frame.size()), message_bytes, cursor);

    memcpy(message_bytes.data() + cursor.position, color_encoder_frame.data(), color_encoder_frame.size());
    cursor.position += color_encoder_frame.size();

    memcpy(message_bytes.data() + cursor.position, depth_encoder_frame.data(), depth_encoder_frame.size());

    return message_bytes;
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
    std::vector<std::byte> packet_bytes(KH_PACKET_SIZE);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(SenderPacketType::Video, packet_bytes, cursor);
    copy_to_bytes(frame_id, packet_bytes, cursor);
    copy_to_bytes(packet_index, packet_bytes, cursor);
    copy_to_bytes(packet_count, packet_bytes, cursor);
    memcpy(packet_bytes.data() + cursor.position, packet_content.data(), packet_content.size());

    return packet_bytes;
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

std::vector<std::byte> merge_video_sender_message_bytes(gsl::span<gsl::span<std::byte>> video_sender_message_data_set)
{
    int message_size{0};
    for (auto& message_data : video_sender_message_data_set)
        message_size += message_data.size();

    std::vector<std::byte> message_bytes(message_size);
    int cursor{0};
    for (auto& message_data : video_sender_message_data_set) {
        memcpy(message_bytes.data() + cursor, message_data.data(), message_data.size());
        cursor += message_data.size();
    }

    return message_bytes;
}

VideoSenderMessageData parse_video_sender_message_bytes(gsl::span<const std::byte> message_bytes)
{
    PacketCursor cursor{};
    VideoSenderMessageData video_sender_message_data;
    copy_from_bytes(video_sender_message_data.frame_time_stamp, message_bytes, cursor);
    copy_from_bytes(video_sender_message_data.keyframe, message_bytes, cursor);

    // Parsing the bytes of the message into the VP8 and TRVL frames.
    int color_encoder_frame_size = copy_from_bytes<int>(message_bytes, cursor);
    int depth_encoder_frame_size = copy_from_bytes<int>(message_bytes, cursor);

    video_sender_message_data.color_encoder_frame = std::vector<std::byte>(color_encoder_frame_size);
    memcpy(video_sender_message_data.color_encoder_frame.data(),
           message_bytes.data() + cursor.position,
           color_encoder_frame_size);
    cursor.position += color_encoder_frame_size;

    video_sender_message_data.depth_encoder_frame = std::vector<std::byte>(depth_encoder_frame_size);
    memcpy(video_sender_message_data.depth_encoder_frame.data(),
           message_bytes.data() + cursor.position,
           depth_encoder_frame_size);
    cursor.position += depth_encoder_frame_size;

    return video_sender_message_data;
}

// This creates xor packets for forward error correction. In case max_group_size is 10, the first XOR FEC packet
// is for packet 0~9. If one of them is missing, it uses XOR FEC packet, which has the XOR result of all those
// packets to restore the packet.
std::vector<std::vector<std::byte>> create_parity_sender_packet_bytes_set(int session_id, int frame_id, int parity_group_size,
                                                                       gsl::span<const std::vector<std::byte>> video_packet_bytes_span)
{
    // For example, when max_group_size = 10, 4 -> 1, 10 -> 1, 11 -> 2.
    const int parity_packet_count{gsl::narrow_cast<int>(video_packet_bytes_span.size() - 1) / parity_group_size + 1};

    std::vector<std::vector<std::byte>> frame_packet_bytes_set;
    for (gsl::index parity_packet_index{0}; parity_packet_index < parity_packet_count; ++parity_packet_index) {
        const int frame_packet_bytes_cursor{gsl::narrow<int>(parity_packet_index * parity_group_size)};
        const int parity_frame_packet_count{std::min<int>(parity_group_size, video_packet_bytes_span.size() - frame_packet_bytes_cursor)};
        frame_packet_bytes_set.push_back(create_parity_sender_packet_bytes(session_id, frame_id, parity_packet_index, parity_packet_count,
                                                                           gsl::span<const std::vector<std::byte>>(&video_packet_bytes_span[frame_packet_bytes_cursor],
                                                                                                                   parity_frame_packet_count)));
    }
    return frame_packet_bytes_set;
}

std::vector<std::byte> create_parity_sender_packet_bytes(int session_id, int frame_id, int packet_index, int packet_count,
                                                      gsl::span<const std::vector<std::byte>> frame_packet_bytes_set)
{
    // Copy packets[begin_index] instead of filling in everything zero
    // to reduce an XOR operation for contents once.
    std::vector<std::byte> packet_bytes{frame_packet_bytes_set[0]};
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(SenderPacketType::Parity, packet_bytes, cursor);
    copy_to_bytes(frame_id, packet_bytes, cursor);
    copy_to_bytes(packet_index, packet_bytes, cursor);
    copy_to_bytes(packet_count, packet_bytes, cursor);

    for (gsl::index i{1}; i < frame_packet_bytes_set.size(); ++i) {
        for (gsl::index j{KH_VIDEO_PACKET_HEADER_SIZE}; j < KH_PACKET_SIZE; ++j) {
            packet_bytes[j] ^= frame_packet_bytes_set[i][j];
        }
    }

    return packet_bytes;
}

ParitySenderPacketData parse_parity_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{5};
    ParitySenderPacketData parity_sender_packet_data;
    copy_from_bytes(parity_sender_packet_data.frame_id, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet_data.packet_index, packet_bytes, cursor);
    copy_from_bytes(parity_sender_packet_data.packet_count, packet_bytes, cursor);

    parity_sender_packet_data.bytes.resize(packet_bytes.size() - cursor.position);
    memcpy(parity_sender_packet_data.bytes.data(),
           &packet_bytes[cursor.position],
           parity_sender_packet_data.bytes.size());

    return parity_sender_packet_data;
}

std::vector<std::byte> create_audio_sender_packet_bytes(int session_id, int frame_id,
                                                        gsl::span<const std::byte> opus_frame)
{
    const int packet_size{gsl::narrow_cast<int>(sizeof(session_id) +
                                                sizeof(SenderPacketType) +
                                                sizeof(frame_id) +
                                                opus_frame.size())};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(SenderPacketType::Audio, packet_bytes, cursor);
    copy_to_bytes(frame_id, packet_bytes, cursor);

    memcpy(packet_bytes.data() + cursor.position, opus_frame.data(), opus_frame.size());

    return packet_bytes;
}

AudioSenderPacketData parse_audio_sender_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    PacketCursor cursor{5};
    AudioSenderPacketData audio_sender_packet_data;
    copy_from_bytes(audio_sender_packet_data.frame_id, packet_bytes, cursor);

    audio_sender_packet_data.opus_frame.resize(packet_bytes.size() - cursor.position);
    memcpy(audio_sender_packet_data.opus_frame.data(),
           packet_bytes.data() + cursor.position,
           audio_sender_packet_data.opus_frame.size());

    return audio_sender_packet_data;
}

std::vector<std::byte> create_floor_sender_packet_bytes(int session_id, float a, float b, float c, float d)
{
    const int packet_size{gsl::narrow_cast<int>(sizeof(session_id) +
                                                sizeof(SenderPacketType) +
                                                sizeof(a) +
                                                sizeof(b) +
                                                sizeof(c) +
                                                sizeof(d))};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(SenderPacketType::Floor, packet_bytes, cursor);
    copy_to_bytes(a, packet_bytes, cursor);
    copy_to_bytes(b, packet_bytes, cursor);
    copy_to_bytes(c, packet_bytes, cursor);
    copy_to_bytes(d, packet_bytes, cursor);

    return packet_bytes;
}

int get_session_id_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<int>(packet_bytes, 0);
}

ReceiverPacketType get_packet_type_from_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    return copy_from_bytes<ReceiverPacketType>(packet_bytes, 4);
}

std::vector<std::byte> create_connect_receiver_packet_bytes(int session_id)
{
    const int packet_size{gsl::narrow_cast<int>(sizeof(session_id) +
                                                sizeof(ReceiverPacketType))};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(ReceiverPacketType::Connect, packet_bytes, cursor);

    return packet_bytes;
}

std::vector<std::byte> create_report_receiver_packet_bytes(int session_id, int frame_id, float decoder_time_ms, float frame_time_ms)
{
    const int packet_size{gsl::narrow_cast<int>(sizeof(session_id) +
                                                sizeof(ReceiverPacketType) +
                                                sizeof(frame_id) +
                                                sizeof(decoder_time_ms) +
                                                sizeof(frame_time_ms))};

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(ReceiverPacketType::Report, packet_bytes, cursor);
    copy_to_bytes(frame_id, packet_bytes, cursor);
    copy_to_bytes(decoder_time_ms, packet_bytes, cursor);
    copy_to_bytes(frame_time_ms, packet_bytes, cursor);

    return packet_bytes;
}

ReportReceiverPacketData parse_report_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    ReportReceiverPacketData report_receiver_packet_data;
    PacketCursor cursor{5};
    copy_from_bytes(report_receiver_packet_data.frame_id, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet_data.decoder_time_ms, packet_bytes, cursor);
    copy_from_bytes(report_receiver_packet_data.frame_time_ms, packet_bytes, cursor);

    return report_receiver_packet_data;
}

std::vector<std::byte> create_request_receiver_packet_bytes(int session_id, int frame_id, const std::vector<int>& packet_indices)
{
    const int packet_size(sizeof(session_id) +
                          sizeof(ReceiverPacketType) +
                          sizeof(frame_id) +
                          sizeof(int) * packet_indices.size());

    std::vector<std::byte> packet_bytes(packet_size);
    PacketCursor cursor;
    copy_to_bytes(session_id, packet_bytes, cursor);
    copy_to_bytes(ReceiverPacketType::Request, packet_bytes, cursor);
    copy_to_bytes(frame_id, packet_bytes, cursor);

    for (int index : packet_indices)
        copy_to_bytes(index, packet_bytes, cursor);

    return packet_bytes;
}

RequestReceiverPacketData parse_request_receiver_packet_bytes(gsl::span<const std::byte> packet_bytes)
{
    RequestReceiverPacketData request_receiver_packet_data;
    PacketCursor cursor{5};
    copy_from_bytes(request_receiver_packet_data.frame_id, packet_bytes, cursor);
    
    int packet_indices_size{gsl::narrow_cast<int>((packet_bytes.size() - cursor.position) / sizeof(int))};
    std::vector<int> packet_indices(packet_indices_size);
    for (int i = 0; i < packet_indices_size; ++i)
        copy_from_bytes(packet_indices[i], packet_bytes, cursor);

    request_receiver_packet_data.packet_indices = packet_indices;

    return request_receiver_packet_data;
}
}
