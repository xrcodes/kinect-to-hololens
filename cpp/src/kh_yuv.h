#pragma once

#include <vector>

extern "C"
{
#include <libavutil/frame.h>
}

namespace kh
{
// An class that contains color pixels in the YUV420 format which Vp8Encoder and Vp8Decoder like.
// Data of this class is not supposed to be copy since it is computationally expensive.
class YuvImage
{
public:
    YuvImage(std::vector<uint8_t>&& y_channel, std::vector<uint8_t>&& u_channel,
        std::vector<uint8_t>&& v_channel, int width, int height)
        : y_channel_{std::move(y_channel)}
        , u_channel_{std::move(u_channel)}
        , v_channel_{std::move(v_channel)}
        , width_(width)
        , height_(height)
    {
    }
    YuvImage(const YuvImage& other) = delete;
    YuvImage& operator=(const YuvImage& other) = delete;
    YuvImage(YuvImage&& other) noexcept
        : y_channel_{std::move(other.y_channel_)}
        , u_channel_{std::move(other.u_channel_)}
        , v_channel_{std::move(other.v_channel_)}
        , width_(other.width_)
        , height_(other.height_)
    {
    }
    YuvImage& operator=(YuvImage&& other) noexcept
    {
        y_channel_ = std::move(other.y_channel_);
        u_channel_ = std::move(other.u_channel_);
        v_channel_ = std::move(other.v_channel_);
        width_ = other.width_;
        height_ = other.height_;
        return *this;
    }
    const std::vector<uint8_t>& y_channel() const { return y_channel_; }
    const std::vector<uint8_t>& u_channel() const { return u_channel_; }
    const std::vector<uint8_t>& v_channel() const { return v_channel_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    std::vector<uint8_t> y_channel_;
    std::vector<uint8_t> u_channel_;
    std::vector<uint8_t> v_channel_;
    int width_;
    int height_;
};

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
    const AVFrame* av_frame() const { return av_frame_; }

private:
    AVFrame* av_frame_;
};

// createYuvImageFromAzureKinectYuy2Buffer(): converts color pixels to a YuvImage.
// createYuvImageFromAvFrame(): converts the outcome of Vp8Decoder to color pixels in Yuv420.
YuvImage createYuvImageFromAzureKinectYuy2Buffer(const uint8_t* buffer, int width, int height, int stride);
YuvImage createYuvImageFromAzureKinectBgraBuffer(const uint8_t* buffer, int width, int height, int stride);
YuvImage createYuvImageFromAvFrame(const AVFrame& av_frame);
}