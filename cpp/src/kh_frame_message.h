#pragma once

#include <vector>

namespace kh
{
class FrameMessage
{
private:
    FrameMessage(std::vector<uint8_t>&& message, int frame_id, float frame_time_stamp,
                 bool keyframe, int color_encoder_frame_size, int depth_encoder_frame_size);

public:
    static FrameMessage create(int frame_id, std::vector<uint8_t>&& message);
    int frame_id() const { return frame_id_; }
    float frame_time_stamp() const { return frame_time_stamp_; }
    bool keyframe() const { return keyframe_; }
    int color_encoder_frame_size() const { return color_encoder_frame_size_; }
    int depth_encoder_frame_size() const { return depth_encoder_frame_size_; }
    std::vector<uint8_t> getColorEncoderFrame();
    std::vector<uint8_t> getDepthEncoderFrame();

private:
    std::vector<uint8_t> message_;
    int frame_id_;
    float frame_time_stamp_;
    bool keyframe_;
    int color_encoder_frame_size_;
    int depth_encoder_frame_size_;
};
}