#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#pragma warning(push)
#pragma warning(disable: 26812)
#include <vpx/vp8cx.h>
#pragma warning(pop)
#include <gsl/gsl>

namespace kh
{
class YuvFrame;

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

// A wrapper class for libvpx, encoding color pixels into the VP8 codec.
class Vp8Encoder
{
public:
    Vp8Encoder(int width, int height);
    ~Vp8Encoder();
    std::vector<std::byte> encode(const YuvFrame& yuv_image, bool keyframe);

private:
    vpx_codec_ctx_t codec_context_;
    vpx_image_t image_;
    int frame_index_;
};

// A wrapper class for FFMpeg, decoding colors pixels in the VP8 codec.
class Vp8Decoder
{
private:
    class CodecContext;
    class CodecParserContext;
    class Packet;

public:
    Vp8Decoder();
    FFmpegFrame decode(gsl::span<const std::byte> vp8_frame);

private:
    std::shared_ptr<CodecContext> codec_context_;
    std::shared_ptr<CodecParserContext> codec_parser_context_;
    std::shared_ptr<Packet> packet_;
};
}