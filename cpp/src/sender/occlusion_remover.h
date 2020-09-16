#pragma once

#include "native/kh_native.h"
#include "win32/kh_kinect.h"

namespace k4a
{
struct calibration;
}

namespace kh
{
class OcclusionRemover
{
public:
    OcclusionRemover(const k4a::calibration& calibration);
    void remove(gsl::span<int16_t> depth_pixels);

private:
    const int width_;
    const int height_;
    float color_camera_x_;
    std::vector<float> unit_depth_x_;
};
}