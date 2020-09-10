#pragma once

#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

#include <gsl/gsl>

namespace tt
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