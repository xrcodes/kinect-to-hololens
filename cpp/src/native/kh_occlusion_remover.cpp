#include "kh_occlusion_remover.h"

#include <iostream>

namespace kh
{
namespace
{
float get_color_camera_x_from_calibration(k4a::calibration calibration)
{
    return calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0];
}

std::vector<float> create_x_with_unit_depth(const k4a::calibration& calibration)
{
    const int width{calibration.depth_camera_calibration.resolution_width};
    const int height{calibration.depth_camera_calibration.resolution_height};

    std::vector<float> x_with_unit_depth(gsl::narrow_cast<int>(width * height));

    k4a_float3_t point;
    for (gsl::index j{0}; j < height; ++j) {
        for (gsl::index i{0}; i < width; ++i) {
            if (!calibration.convert_2d_to_3d(k4a_float2_t{gsl::narrow_cast<float>(i), gsl::narrow_cast<float>(j)},
                                              1.0f,
                                              K4A_CALIBRATION_TYPE_DEPTH,
                                              K4A_CALIBRATION_TYPE_DEPTH,
                                              &point)) {
                throw std::runtime_error("Failed in PointCloud::createUnitDepthPointCloud");
            }
            x_with_unit_depth[i + j * width] = point.xyz.x;
        }
    }

    return x_with_unit_depth;
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
    , color_camera_x_{get_color_camera_x_from_calibration(calibration)}
    , x_with_unit_depth_{create_x_with_unit_depth(calibration)}
    , x_with_unit_depth_times_c_inv_{multiply_vector(x_with_unit_depth_, 1.0f / color_camera_x_)}
{
}

// Instead of removing pixels in two steps as remove(),
// by ignoring the edge cases, remove2() removes occlusions
// in one step with much less iterations.
void OcclusionRemover::remove(gsl::span<int16_t> depth_pixels)
{
    for (int j{0}; j < height_; ++j) {
        // z_max contains the cutoffs for the j-th row.
        //std::vector<float> z_max(width_, AZURE_KINECT_MAX_DISTANCE);

        for (int i{width_ - 1}; i >= 0; --i) {
            // p stands for point.
            const int p_index{i + j * width_};
            const int16_t z{depth_pixels[p_index]};

            // Skip invalid pixels.
            if (z == 0)
                continue;

            const float x{x_with_unit_depth_[p_index]};
            for (int ii{i - 1}; ii >= 0; --ii) {
                const int pp_index{ii + j * width_};
                const int16_t zz{depth_pixels[pp_index]};
                if (zz == 0)
                    continue;

                const float xx{x_with_unit_depth_[pp_index]};
                const float line_z{(color_camera_x_ * z) / ((xx - x) * z + color_camera_x_)};

                // When the gap between x and xx becomes too large, (x - xx) * z > color_camera_x_,
                // line_z becomes negative indicating that physically there can be no occlusions.
                // And, of course, if a point is not blocked, it should not be invalidated.
                if ((zz < line_z) || (line_z < 0)) {
                    // Equivalent to setting i to ii for the next loop with i.
                    // Skipping loops that only finds i is for an invalid pixel.
                    //i = ii + 1;
                    break;
                }

                // If line_z covers an existing pixel pp, invalidate pp.
                depth_pixels[pp_index] = 0;
            }
        }
    }
}
}