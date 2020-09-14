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


// An FFMPEG wrapper class that decodes VP8 frames.
class Vp8Decoder
{
private:
    typedef std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext*)>> AVCodecContextHandle;
    typedef std::unique_ptr<AVCodecParserContext, std::function<void(AVCodecParserContext*)>> AVCodecParserContextHandle;
    typedef std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> AVPacketHandle;

public:
    Vp8Decoder();
    FFmpegFrame decode(gsl::span<const std::byte> vp8_frame);
private:
    void decode_packet(AVCodecContextHandle& codec_context, AVPacketHandle& packet, std::vector<FFmpegFrame>& decoder_frames);

private:
    AVCodecContextHandle codec_context_;
    AVCodecParserContextHandle codec_parser_context_;
    AVPacketHandle packet_;
};
}