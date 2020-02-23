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
    std::vector<std::byte> encode(const int16_t* depth_buffer, bool keyframe);

private:
    std::vector<TrvlPixel> pixels_;
    short change_threshold_;
    int invalid_threshold_;
};

class TrvlDecoder
{
public:
    TrvlDecoder(int frame_size);
    std::vector<int16_t> decode(const std::byte* trvl_frame, bool keyframe);

private:
    // Using int16_t to be compatible with the differences that can have negative values.
    std::vector<int16_t> prev_pixel_values_;
};
}