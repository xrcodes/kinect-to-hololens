#pragma once

#include <vector>
#include <gsl/gsl>

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
    // 3.86 m is the operating range of NFOV unbinned mode of Azure Kinect.
    // But larger values can be detected, so modified it to 10 m...
    //constexpr static float AZURE_KINECT_MAX_DISTANCE{3860.0f};
    constexpr static float AZURE_KINECT_MAX_DISTANCE{10000.0f};
    constexpr static float AZURE_KINECT_MAX_DISTANCE_INV{1.0f / AZURE_KINECT_MAX_DISTANCE};
    const int width_;
    const int height_;
    float color_camera_x_;
    std::vector<float> x_with_unit_depth_;
    std::vector<float> x_with_unit_depth_times_c_inv_;
};
}