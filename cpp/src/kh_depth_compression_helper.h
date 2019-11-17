#include "kh_rvl.h"
#include "kh_trvl.h"
#include "kh_vp8.h"

// DepthEncoder and DepthDecoder is to encapsulate depth compression algorithms
// into interfaces so the algorithms can be compared.
namespace kh
{
enum class DepthCompressionType
{
    Rvl = 0, Trvl = 1, Vp8 = 2
};

class DepthEncoder
{
public:
    virtual std::vector<uint8_t> encode(short* depth_buffer) = 0;
};

class DepthDecoder
{
public:
    virtual std::vector<short> decode(uint8_t* depth_encoder_frame, size_t depth_encoder_frame_size) = 0;
};

class RvlDepthEncoder : public DepthEncoder
{
public:
    RvlDepthEncoder(int frame_size)
        : frame_size_(frame_size)
    {
    }
    std::vector<uint8_t> encode(short* depth_buffer)
    {
        return rvl::compress(depth_buffer, frame_size_);
    }

private:
    int frame_size_;
};

class RvlDepthDecoder : public DepthDecoder
{
public:
    RvlDepthDecoder(int frame_size)
        : frame_size_(frame_size)
    {
    }
    std::vector<short> decode(uint8_t* depth_encoder_frame, size_t depth_encoder_frame_size)
    {
        return rvl::decompress(depth_encoder_frame, frame_size_);
    }

private:
    int frame_size_;
};

class TrvlDepthEncoder : public DepthEncoder
{
public:
    TrvlDepthEncoder(int frame_size, short change_threshold, int invalid_threshold)
        : trvl_encoder_(frame_size, change_threshold, invalid_threshold)
    {
    }
    std::vector<uint8_t> encode(short* depth_buffer)
    {
        return trvl_encoder_.encode(depth_buffer);
    }

private:
    TrvlEncoder trvl_encoder_;
};

class TrvlDepthDecoder : public DepthDecoder
{
public:
    TrvlDepthDecoder(int frame_size)
        : trvl_decoder_(frame_size)
    {
    }
    std::vector<short> decode(uint8_t* depth_encoder_frame, size_t depth_encoder_frame_size)
    {
        return trvl_decoder_.decode(depth_encoder_frame);
    }

private:
    TrvlDecoder trvl_decoder_;
};

class Vp8DepthEncoder : public DepthEncoder
{
public:
    Vp8DepthEncoder(int width, int height, int target_bitrate)
        : vp8_encoder_(width, height, target_bitrate), width_(width), height_(height)
    {
    }
    std::vector<uint8_t> encode(short* depth_buffer)
    {
        int frame_size = width_ * height_;
        std::vector<uint8_t> y_channel(frame_size);
        for (int i = 0; i < frame_size; ++i)
            y_channel[i] = depth_buffer[i] / 16;
        std::vector<uint8_t> u_channel(frame_size / 4, 128);
        std::vector<uint8_t> v_channel(frame_size / 4, 128);
        YuvImage yuv_image(std::move(y_channel), std::move(u_channel), std::move(v_channel), width_, height_);
        return vp8_encoder_.encode(yuv_image);
    }

private:
    Vp8Encoder vp8_encoder_;
    int width_;
    int height_;
};

class Vp8DepthDecoder : public DepthDecoder
{
public:
    Vp8DepthDecoder()
    {
    }
    std::vector<short> decode(uint8_t* depth_encoder_frame, size_t depth_encoder_frame_size)
    {
        auto ffmpeg_frame = vp8_decoder_.decode(depth_encoder_frame, depth_encoder_frame_size);

        int width = ffmpeg_frame.av_frame()->width;
        int height = ffmpeg_frame.av_frame()->height;
        auto data = ffmpeg_frame.av_frame()->data[0];
        auto line_size = ffmpeg_frame.av_frame()->linesize[0];

        std::vector<short> depth_pixels(width * height);
        for (int j = 0; j < height; ++j) {
            for (int i = 0; i < width; ++i) {
                depth_pixels[i + j * width] = data[i + j * line_size] * 16;
            }
        }

        return depth_pixels;
    }

private:
    Vp8Decoder vp8_decoder_;
};
}