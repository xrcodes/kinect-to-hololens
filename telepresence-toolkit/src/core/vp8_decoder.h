#pragma once

#include <gsl/gsl>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
}

namespace tt
{
typedef std::unique_ptr<AVFrame, std::function<void(AVFrame*)>> AVFrameHandle;

class Vp8Decoder
{
private:
    typedef std::unique_ptr<AVCodecContext, std::function<void(AVCodecContext*)>> AVCodecContextHandle;
    typedef std::unique_ptr<AVCodecParserContext, std::function<void(AVCodecParserContext*)>> AVCodecParserContextHandle;
    typedef std::unique_ptr<AVPacket, std::function<void(AVPacket*)>> AVPacketHandle;

public:
    Vp8Decoder();
    AVFrameHandle decode(gsl::span<const std::byte> vp8_frame);

private:
    AVCodecContextHandle codec_context_;
    AVCodecParserContextHandle codec_parser_context_;
    AVPacketHandle packet_;
};
}