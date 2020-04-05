#pragma once

#include <vector>
#include <gsl/gsl>
#include <k4a/k4a.hpp>

namespace kh
{
class ShadowRemover
{
public:
    ShadowRemover(const k4a::calibration& calibration);
    void remove(gsl::span<int16_t> depth_pixels);

private:
    const int width_;
    const int height_;
    float color_camera_x_;
    std::vector<float> x_with_unit_depth_;
};
}