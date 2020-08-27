#pragma once

#include <vector>

namespace kh
{
class FFmpegFrame;

// An class that contains color pixels in the YUV420 format which Vp8Encoder and Vp8Decoder like.
// Data of this class is not supposed to be copy since it is computationally expensive.
class YuvFrame
{
public:
    YuvFrame(std::vector<uint8_t>&& y_channel, std::vector<uint8_t>&& u_channel,
        std::vector<uint8_t>&& v_channel, int width, int height)
        : y_channel_{std::move(y_channel)}
        , u_channel_{std::move(u_channel)}
        , v_channel_{std::move(v_channel)}
        , width_(width)
        , height_(height)
    {
    }
    YuvFrame(const YuvFrame& other) = delete;
    YuvFrame& operator=(const YuvFrame& other) = delete;
    YuvFrame(YuvFrame&& other) noexcept
        : y_channel_{std::move(other.y_channel_)}
        , u_channel_{std::move(other.u_channel_)}
        , v_channel_{std::move(other.v_channel_)}
        , width_(other.width_)
        , height_(other.height_)
    {
    }
    YuvFrame& operator=(YuvFrame&& other) noexcept
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

// createYuvImageFromAzureKinectYuy2Buffer(): converts color pixels to a YuvImage.
// createYuvImageFromAvFrame(): converts the outcome of Vp8Decoder to color pixels in Yuv420.
YuvFrame createYuvFrameFromAzureKinectYuy2Buffer(const uint8_t* buffer, int width, int height, int stride);
YuvFrame createYuvFrameFromAzureKinectBgraBuffer(const uint8_t* buffer, int width, int height, int stride);
YuvFrame createYuvFrameFromFFmpegFrame(FFmpegFrame& ffmpeg_frame);
}