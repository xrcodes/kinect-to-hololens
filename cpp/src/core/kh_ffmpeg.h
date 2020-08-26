#pragma once

extern "C"
{
#include <libavutil/frame.h>
}

namespace kh
{
// A wrapper for AVFrame, the outcome of Vp8Decoder.
class FFmpegFrame
{
public:
    FFmpegFrame(AVFrame* av_frame)
        : av_frame_(av_frame)
    {
    }
    ~FFmpegFrame()
    {
        if (av_frame_)
            av_frame_free(&av_frame_);
    }
    FFmpegFrame(const FFmpegFrame& other) = delete;
    FFmpegFrame& operator=(const FFmpegFrame& other) = delete;
    FFmpegFrame(FFmpegFrame&& other) noexcept
        : av_frame_(other.av_frame_)
    {
        other.av_frame_ = nullptr;
    }
    FFmpegFrame& operator=(FFmpegFrame&& other) noexcept
    {
        if (av_frame_)
            av_frame_free(&av_frame_);

        av_frame_ = other.av_frame_;
        other.av_frame_ = nullptr;
        return *this;
    }
    AVFrame* av_frame() const { return av_frame_; }

private:
    AVFrame* av_frame_;
};
}