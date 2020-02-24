#pragma once

#include <vector>
#include <gsl/gsl>

namespace kh
{
struct TrvlPixel
{
    std::int16_t value{0};
    int invalid_count{0};
};

class TrvlEncoder
{
public:
    TrvlEncoder(int frame_size, std::int16_t change_threshold, int invalid_threshold);
    std::vector<std::byte> encode(gsl::span<const std::int16_t> depth_buffer, bool keyframe);

private:
    std::vector<TrvlPixel> pixels_;
    std::int16_t change_threshold_;
    int invalid_threshold_;
};

class TrvlDecoder
{
public:
    TrvlDecoder(int frame_size);
    std::vector<int16_t> decode(gsl::span<const std::byte> trvl_frame, bool keyframe) noexcept;

private:
    // Using int16_t to be compatible with the differences that can have negative values.
    std::vector<int16_t> prev_pixel_values_;
};
}