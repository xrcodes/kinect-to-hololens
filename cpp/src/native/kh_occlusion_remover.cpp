#include "kh_occlusion_remover.h"

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

// All the inverses are to remove divisions from the calculation of zz.
// Note that float divisions takes way longer than other operations.
// The inner loop with ii in average had about 20 times of repetition per pixel in average
// from a scientific experiment from my dorm room.
// This modification reduced 25% of the computation time.
// Using int instead of gsl::index to minimize casting.
void OcclusionRemover::remove(gsl::span<int16_t> depth_pixels)
{
    // 3.86 m is the operating range of NFOV unbinned mode of Azure Kinect.
    const float c_inv{1.0f / color_camera_x_};

    //int invalidation_count = 0;
    //int z_max_update_count = 0;
    for (int j{0}; j < height_; ++j) {
        // z_max contains the cutoffs for the j-th row.
        //std::vector<float> z_max(width, AZURE_KINECT_MAX_DISTANCE);
        std::vector<float> z_max_inv(width_, AZURE_KINECT_MAX_DISTANCE_INV);
        const int p_index_offset{j * width_};

        for (int i{width_ - 1}; i >= 0; --i) {
            // p stands for point.
            //const int p_index{i + j * width_};
            const int p_index{i + p_index_offset};
            const int16_t z{depth_pixels[p_index]};

            // Skip invalid pixels.
            if (z == 0)
                continue;

            // Invalidate the pixel if it is covered by another pixel.
            //if (z > z_max[i]) {
            //if (z > (1.0f / z_max_inv[i])) {
            if (z * z_max_inv[i] > 1.0f) {
                depth_pixels[p_index] = 0;
                //++invalidation_count;
                continue;
            }

            //auto p{unit_depth_point_cloud_.points[p_index].xyz};
            //const float x{p.x};
            const float x{x_with_unit_depth_[p_index]};
            const float z_inv{1.0f / z};
            const float zz_inv_offset{-x * c_inv + z_inv};
            const int pp_index_offset{j * width_};

            for (int ii{i - 1}; ii >= 0; --ii) {
                //gsl::index pp_index{ii + j * width};
                //auto pp{unit_depth_point_cloud_.points[pp_index].xyz};
                //const float xx{pp.x};
                //const float xx{unit_depth_point_cloud_.points[pp_index].xyz.x};
                //const float xx{x_with_unit_depth_[ii + j * width_]};
                //const float xx_times_max_distance_inv_{x_with_unit_depth_times_c_inv_[ii + j * width_]};
                const float xx_times_max_distance_inv_{x_with_unit_depth_times_c_inv_[ii + pp_index_offset]};
                //const float zz{(color_camera_x_ * z) / ((xx - x) * z + color_camera_x_)};
                //const float zz{1.0f / ((xx - x) * c_inv + z_inv)};
                //const float zz_inv{(xx - x) * c_inv + z_inv};
                const float zz_inv{xx_times_max_distance_inv_ + zz_inv_offset};

                //if (zz >= z_max[ii])
                if (zz_inv <= z_max_inv[ii])
                    break;

                // If zz covers new area, update z_max that indicates the area covered
                // and continue to the next pixels more on the right side.
                z_max_inv[ii] = zz_inv;
                //++z_max_update_count;
            }
        }
    }
    //std::cout << "invalidation_count: " << invalidation_count << ","
    //          << "z_max_update_count: " << z_max_update_count << "\n";
}

// The version without optimization.
void OcclusionRemover::remove_original(gsl::span<int16_t> depth_pixels)
{
    for (int j{0}; j < height_; ++j) {
        // z_max contains the cutoffs for the j-th row.
        std::vector<float> z_max(width_, AZURE_KINECT_MAX_DISTANCE);

        for (int i{width_ - 1}; i >= 0; --i) {
            // p stands for point.
            const int p_index{i + j * width_};
            const int16_t z{depth_pixels[p_index]};

            // Skip invalid pixels.
            if (z == 0)
                continue;

            // Invalidate the pixel if it is covered by another pixel.
            if (z > z_max[i]) {
                depth_pixels[p_index] = 0;
                continue;
            }

            const float x{x_with_unit_depth_[p_index]};
            for (int ii{i - 1}; ii >= 0; --ii) {
                const float xx{x_with_unit_depth_[ii + j * width_]};
                const float zz{(color_camera_x_ * z) / ((xx - x) * z + color_camera_x_)};

                if (zz >= z_max[ii])
                    break;

                // If zz covers new area, update z_max that indicates the area covered
                // and continue to the next pixels more on the right side.
                z_max[ii] = zz;
            }
        }
    }
    //std::cout << "invalidation_count: " << invalidation_count << ","
    //          << "z_max_update_count: " << z_max_update_count << "\n";
}
}