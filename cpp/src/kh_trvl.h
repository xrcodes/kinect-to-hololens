#pragma once

#include <vector>

namespace kh
{
struct TrvlPixel
{
public:
    TrvlPixel() : value(0), invalid_count(0) {}
    short value;
    int invalid_count;
};

class TrvlEncoder
{
public:
    TrvlEncoder(int frame_size, short change_threshold, int invalid_threshold);
    std::vector<uint8_t> encode(short* depth_buffer, bool keyframe);

private:
    std::vector<TrvlPixel> pixels_;
    short change_threshold_;
    int invalid_threshold_;
};

class TrvlDecoder
{
public:
    TrvlDecoder(int frame_size);
    std::vector<short> decode(uint8_t* trvl_frame, bool keyframe);

private:
    std::vector<short> prev_pixel_values_;
};
}