//#pragma once
//
//#include <chrono>
//#include <vector>
//#include "kh_time.h"
//
//namespace kh
//{
//class VideoMessage
//{
//public:
//    VideoMessage();
//
//private:
//    VideoMessage(std::vector<std::byte>&& message, int frame_id, float frame_time_stamp,
//                 bool keyframe, int color_encoder_frame_size, int depth_encoder_frame_size);
//
//public:
//    static VideoMessage create(int frame_id, std::vector<std::byte>&& message);
//    int frame_id() const { return frame_id_; }
//    float frame_time_stamp() const { return frame_time_stamp_; }
//    bool keyframe() const { return keyframe_; }
//    int color_encoder_frame_size() const { return color_encoder_frame_size_; }
//    int depth_encoder_frame_size() const { return depth_encoder_frame_size_; }
//    std::vector<std::byte> getColorEncoderFrame();
//    std::vector<std::byte> getDepthEncoderFrame();
//
//private:
//    std::vector<std::byte> message_;
//    int frame_id_;
//    float frame_time_stamp_;
//    bool keyframe_;
//    int color_encoder_frame_size_;
//    int depth_encoder_frame_size_;
//};
//}