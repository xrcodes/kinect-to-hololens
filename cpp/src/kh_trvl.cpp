#include "kh_trvl.h"

#include "kh_rvl.h"

namespace kh
{
uint16_t abs_diff(short x, short y)
{
    if (x > y)
        return x - y;
    else
        return y - x;
}

void update_pixel(TrvlPixel& pixel, short raw_value, short change_threshold, int invalidation_threshold) {
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
    if (abs_diff(pixel.value, raw_value) > change_threshold)
        pixel.value = raw_value;
}

TrvlEncoder::TrvlEncoder(int frame_size, short change_threshold, int invalid_threshold)
    : pixels_(frame_size), prev_pixel_values_(frame_size, 0), 
    change_threshold_(change_threshold), invalid_threshold_(invalid_threshold)
{
}

std::vector<char> TrvlEncoder::encode(short* depth_buffer)
{
    auto frame_size = pixels_.size();
    std::vector<short> pixel_diffs(frame_size);
    for (int i = 0; i < frame_size; ++i) {
        update_pixel(pixels_[i], depth_buffer[i], change_threshold_, invalid_threshold_);
        pixel_diffs[i] = pixels_[i].value - prev_pixel_values_[i];
        prev_pixel_values_[i] = pixels_[i].value;
    }

    return rvl::compress(pixel_diffs.data(), frame_size);
}

TrvlDecoder::TrvlDecoder(int frame_size)
    : prev_pixel_values_(frame_size, 0)
{
}
std::vector<short> TrvlDecoder::decode(char* trvl_frame)
{
    int frame_size = prev_pixel_values_.size();
    auto pixel_diffs = rvl::decompress(trvl_frame, frame_size);
    for (int i = 0; i < frame_size; ++i)
        prev_pixel_values_[i] += pixel_diffs[i];

    return prev_pixel_values_;
}
}
