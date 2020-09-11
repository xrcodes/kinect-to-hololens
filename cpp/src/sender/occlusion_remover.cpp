#include "occlusion_remover.h"

#include <iostream>

namespace kh
{
namespace
{
float get_color_camera_x(k4a::calibration calibration)
{
    return calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0];
}

std::vector<float> compute_unit_depth_x(const k4a::calibration& calibration)
{
    const int width{calibration.depth_camera_calibration.resolution_width};
    const int height{calibration.depth_camera_calibration.resolution_height};

    std::vector<float> unit_depth_x(gsl::narrow<int>(width * height));

    k4a_float3_t point;
    for (gsl::index j{0}; j < height; ++j) {
        for (gsl::index i{0}; i < width; ++i) {
            if (!calibration.convert_2d_to_3d(k4a_float2_t{gsl::narrow<float>(i), gsl::narrow<float>(j)},
                                              1.0f,
                                              K4A_CALIBRATION_TYPE_DEPTH,
                                              K4A_CALIBRATION_TYPE_DEPTH,
                                              &point)) {
                throw std::runtime_error("Failed projecting a depth pixel in OcclusionRemover::compute_unit_depth_x().");
            }
            unit_depth_x[i + j * width] = point.xyz.x;
        }
    }

    return unit_depth_x;
}

std::vector<float> multiply_vector(const std::vector<float>& v, float multiplier)
{
    std::vector<float> vv(v.size());
    for (int i = 0; i < v.size(); ++i)
        vv[i] = v[i] * multiplier;
    return vv;
}
}

OcclusionRemover::OcclusionRemover(const k4a::calibration& calibration)
    : width_{calibration.depth_camera_calibration.resolution_width}
    , height_{calibration.depth_camera_calibration.resolution_height}
    , color_camera_x_{get_color_camera_x(calibration)}
    , unit_depth_x_{compute_unit_depth_x(calibration)}
{
}

void OcclusionRemover::remove(gsl::span<int16_t> depth_pixels)
{
    for (int col{0}; col < height_; ++col) {
        for (int i{width_ - 1}; i >= 0; --i) {
            const int ii{i + col * width_};
            const int16_t d_i{depth_pixels[ii]};

            // Skip invalid pixels.
            if (d_i == 0)
                continue;

            const float u_i{unit_depth_x_[ii]};
            for (int j{i - 1}; j >= 0; --j) {
                const int jj{j + col * width_};
                const int16_t d_j{depth_pixels[jj]};

                // Skip invalid pixels.
                if (d_j == 0)
                    continue;

                const float u_j{unit_depth_x_[jj]};
                const float d_prime{(color_camera_x_ * d_i) / ((u_j - u_i) * d_i + color_camera_x_)};

                // d_j < d_prime indicates validity of the j-th pixel.
                // When the gap between u_i and u_j becomes too large,
                // (u_j - u_i) * d_i + color_camera_x_ can become negative
                // and make d_prime negative, indicating that there can be no occlusions.
                if ((d_j < d_prime) || (d_prime < 0)) {
                    break;
                }

                depth_pixels[jj] = 0;
            }
        }
    }
}
}