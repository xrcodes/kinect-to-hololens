#pragma once

#include "win32/opencv_utils.h"

namespace kh
{
class VideoRenderer
{
public:
    VideoRenderer(int width, int height)
        : width_{width}
        , height_{height}
        , color_decoder_{}
        , depth_decoder_{width * height}
    {
    }

    void render(std::vector<std::byte>& color_encoder_frame, std::vector<std::byte>& depth_encoder_frame, bool keyframe)
    {
        tt::AVFrameHandle av_frame{color_decoder_.decode(color_encoder_frame)};
        std::vector<int16_t> trvl_frame{depth_decoder_.decode(depth_encoder_frame, keyframe)};

        auto color_mat{create_cv_mat_from_yuv_image(tt::YuvFrame::create(av_frame))};
        auto depth_mat{create_cv_mat_from_kinect_depth_image(trvl_frame.data(), width_, height_)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            return;
    }

private:
    int width_;
    int height_;
    tt::Vp8Decoder color_decoder_;
    tt::TrvlDecoder depth_decoder_;
    std::optional<int> last_frame_id_;
};
}