#include "kh_trvl.h"

#include "kh_rvl.h"

namespace kh
{
namespace
{
std::int16_t absolute_difference(std::int16_t x, std::int16_t y)
{
    if (x > y)
        return x - y;
    else
        return y - x;
}

void update_pixel(TrvlPixel& pixel, const std::int16_t raw_value, const std::int16_t change_threshold, const int invalidation_threshold)
{
    if (pixel.value == 0) {
        if (raw_value > 0)
            pixel.value = raw_value;

        return;
    }

    // Reset the pixel if the depth value indicates the input was invalid two times in a row.
    if (raw_value == 0) {
        ++pixel.invalid_count;
        if (pixel.invalid_count >= invalidation_threshold) {
            pixel.value = 0;
            pixel.invalid_count = 0;
        }
        return;
    }

    pixel.invalid_count = 0;

    // Update pixel value when change is detected.
    if (absolute_difference(pixel.value, raw_value) > change_threshold)
        pixel.value = raw_value;
}
}

TrvlEncoder::TrvlEncoder(int pixel_count, int16_t change_threshold, int invalid_threshold)
    : pixels_(pixel_count), change_threshold_{change_threshold}, invalid_threshold_{invalid_threshold}
{
}

std::vector<std::byte> TrvlEncoder::encode(gsl::span<const int16_t> depth_buffer, bool keyframe)
{
    const int frame_size{gsl::narrow_cast<int>(pixels_.size())};
    if (keyframe) {
        for (gsl::index i{0}; i < frame_size; ++i) {
            pixels_[i].value = depth_buffer[i];
            // equivalent to depth_buffer[i] == 0 ? 1: 0
            pixels_[i].invalid_count = static_cast<int>(depth_buffer[i] == 0);
        }

        return rvl::compress(depth_buffer, frame_size);
    }

    std::vector<short> pixel_diffs(frame_size);
    for (gsl::index i{0}; i < frame_size; ++i) {
        pixel_diffs[i] = pixels_[i].value;
        update_pixel(pixels_[i], depth_buffer[i], change_threshold_, invalid_threshold_);
        pixel_diffs[i] = pixels_[i].value - pixel_diffs[i];
    }

    return rvl::compress(pixel_diffs, frame_size);
}

TrvlDecoder::TrvlDecoder(int pixel_count)
    : prev_pixel_values_(pixel_count, 0)
{
}

std::vector<int16_t> TrvlDecoder::decode(gsl::span<const std::byte> trvl_frame, bool keyframe) noexcept
{
    const int pixel_count{gsl::narrow_cast<int>(prev_pixel_values_.size())};
    if (keyframe) {
        prev_pixel_values_ = rvl::decompress(trvl_frame, pixel_count);
        return prev_pixel_values_;
    }

    const auto pixel_diffs{rvl::decompress(trvl_frame, pixel_count)};
    for (gsl::index i{0}; i < pixel_count; ++i)
        prev_pixel_values_[i] += pixel_diffs[i];

    return prev_pixel_values_;
}
}
