#pragma once

#include <vpx/vp8cx.h>
#include <vpx/vpx_codec.h>
#include "kh_core.h"

namespace kh
{

// A wrapper class for libvpx, encoding color pixels into the VP8 codec.
class Vp8Encoder
{
public:
    Vp8Encoder(int width, int height, int target_bitrate);
    ~Vp8Encoder();
    std::vector<uint8_t> encode(YuvImage& yuv_image, bool keyframe);

private:
    vpx_codec_ctx_t codec_;
    vpx_image_t image_;
    int frame_index_;
};

namespace raii
{
class CodecContext
{
public:
    CodecContext(AVCodec* codec)
        : codec_context_{avcodec_alloc_context3(codec)}
    {
        if (!codec_context_)
            throw std::exception("avcodec_alloc_context3 failed.");
    }

    ~CodecContext()
    {
        if (codec_context_)
            avcodec_free_context(&codec_context_);
    }

    AVCodecContext* get() noexcept { return codec_context_; }

private:
    AVCodecContext* codec_context_;
};

class CodecParserContext
{
public:
    CodecParserContext(int codec_id)
        : codec_parser_context_{av_parser_init(codec_id)}
    {
        if (!codec_parser_context_)
            throw std::exception("av_parser_init failed from CodecParserContext::CodecParserContext");
    }

    ~CodecParserContext()
    {
        if (codec_parser_context_)
            av_parser_close(codec_parser_context_);
    }

    AVCodecParserContext* get() noexcept { return codec_parser_context_; }

private:
    AVCodecParserContext* codec_parser_context_;
};

class Packet
{
public:
    Packet()
        : packet_{av_packet_alloc()}
    {
        if (!packet_)
            throw std::exception("av_packet_alloc failed.");
    }

    ~Packet()
    {
        if (packet_)
            av_packet_free(&packet_);
    }

    AVPacket* get() noexcept { return packet_; }

private:
    AVPacket* packet_;
};
}
// A wrapper class for FFMpeg, decoding colors pixels in the VP8 codec.
class Vp8Decoder
{
public:
    Vp8Decoder();
    FFmpegFrame decode(uint8_t* vp8_frame_data, size_t vp8_frame_size);

private:
    raii::CodecContext codec_context_;
    raii::CodecParserContext codec_parser_context_;
    raii::Packet packet_;
};
}