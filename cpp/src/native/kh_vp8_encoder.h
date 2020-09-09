#pragma once

#include <memory>

#pragma warning(push)
#pragma warning(disable: 26812)
#include <vpx/vp8cx.h>
#pragma warning(pop)
#include <gsl/gsl>
#include "core/kh_yuv.h"

namespace kh
{
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
}