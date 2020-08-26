#pragma once

#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <vpx/vp8cx.h>
#include <vpx/vpx_codec.h>
#include <gsl/gsl>
#include "kh_yuv.h"

namespace kh
{

// A wrapper class for libvpx, encoding color pixels into the VP8 codec.
class Vp8Encoder
{
public:
    Vp8Encoder(int width, int height);
    ~Vp8Encoder();
    std::vector<std::byte> encode(const YuvImage& yuv_image, bool keyframe);

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