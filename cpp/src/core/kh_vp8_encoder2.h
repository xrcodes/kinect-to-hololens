#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}
#include "core/kh_yuv.h"

namespace kh
{
// A wrapper class for libvpx, encoding color pixels into the VP8 codec.
class Vp8Encoder2
{
public:
    Vp8Encoder2(int width, int height);
    ~Vp8Encoder2();
    std::vector<std::byte> encode(const YuvFrame& yuv_image, bool keyframe);

private:
    AVCodecContext* c;
    AVPacket* pkt;
    AVFrame* frame;
    int frame_index_;
};
}