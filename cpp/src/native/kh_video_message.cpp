#include "kh_video_message.h"

namespace kh
{
VideoMessage::VideoMessage()
{
}

VideoMessage::VideoMessage(std::vector<uint8_t>&& message, int frame_id, float frame_time_stamp,
                           bool keyframe, int color_encoder_frame_size, int depth_encoder_frame_size,
                           TimeDuration packet_collection_time)
    : message_(std::move(message)), frame_id_(frame_id), frame_time_stamp_(frame_time_stamp),
    keyframe_(keyframe), color_encoder_frame_size_(color_encoder_frame_size),
    depth_encoder_frame_size_(depth_encoder_frame_size),
    packet_collection_time_(packet_collection_time)
{
}

VideoMessage VideoMessage::create(int frame_id, std::vector<uint8_t>&& message,
                                  TimeDuration packet_collection_time)
{
    int cursor = 0;

    float frame_time_stamp;
    memcpy(&frame_time_stamp, message.data() + cursor, 4);
    cursor += 4;

    bool keyframe = message[cursor];
    cursor += 1;

    // Parsing the bytes of the message into the VP8 and RVL frames.
    int color_encoder_frame_size;
    memcpy(&color_encoder_frame_size, message.data() + cursor, 4);
    cursor += 4;

    // Bytes of the color_encoder_frame.
    cursor += color_encoder_frame_size;

    int depth_encoder_frame_size;
    memcpy(&depth_encoder_frame_size, message.data() + cursor, 4);

    return VideoMessage(std::move(message), frame_id, frame_time_stamp,
                        keyframe, color_encoder_frame_size,
                        depth_encoder_frame_size, packet_collection_time);
}

std::vector<std::byte> VideoMessage::getColorEncoderFrame()
{
    int cursor = 4 + 1 + 4;
    std::vector<std::byte> color_encoder_frame(color_encoder_frame_size_);
    memcpy(color_encoder_frame.data(), message_.data() + cursor, color_encoder_frame_size_);

    return color_encoder_frame;
}

std::vector<std::byte> VideoMessage::getDepthEncoderFrame()
{
    int cursor = 4 + 1 + 4 + color_encoder_frame_size_ + 4;
    std::vector<std::byte> depth_encoder_frame(depth_encoder_frame_size_);
    memcpy(depth_encoder_frame.data(), message_.data() + cursor, depth_encoder_frame_size_);

    return depth_encoder_frame;
}
}