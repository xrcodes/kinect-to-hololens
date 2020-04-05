#pragma once

struct PointCloud
{
public:
    static PointCloud createUnitDepthPointCloud(const k4a::calibration& calibration)
    {
        PointCloud point_cloud;
        point_cloud.width = calibration.depth_camera_calibration.resolution_width;
        point_cloud.height = calibration.depth_camera_calibration.resolution_height;

        point_cloud.points.resize(gsl::narrow_cast<size_t>(point_cloud.width * point_cloud.height));

        for (gsl::index j{0}; j < point_cloud.height; ++j) {
            for (gsl::index i{0}; i < point_cloud.width; ++i) {
                if (!calibration.convert_2d_to_3d(k4a_float2_t{gsl::narrow_cast<float>(i), gsl::narrow_cast<float>(j)},
                                                  1.0f,
                                                  K4A_CALIBRATION_TYPE_DEPTH,
                                                  K4A_CALIBRATION_TYPE_DEPTH,
                                                  & point_cloud.points[i + j * point_cloud.width])) {
                    throw std::runtime_error("Failed in PointCloud::createUnitDepthPointCloud");
                }
            }
        }
        return point_cloud;
    }

    int width;
    int height;
    std::vector<k4a_float3_t> points;
};

class ShadowRemover
{
public:
    ShadowRemover(PointCloud&& unit_depth_point_cloud, float color_camera_x)
        : unit_depth_point_cloud_{unit_depth_point_cloud}, color_camera_x_{color_camera_x}
    {
    }

    // All the inverses are to remove divisions from the calculation of zz.
    // Note that float divisions takes way longer than other operations.
    // The inner loop with ii in average had about 20 times of repetition per pixel in average
    // from a scientific experiment from my dorm room.
    // This modification reduced 25% of the computation time.
    void remove(gsl::span<int16_t> depth_pixels)
    {
        // 3.86 m is the operating range of NFOV unbinned mode of Azure Kinect.
        constexpr float AZURE_KINECT_MAX_DISTANCE{3860.0f};
        const int width{unit_depth_point_cloud_.width};
        const int height{unit_depth_point_cloud_.height};
        const float c_inv{1.0f / color_camera_x_};

        int invalidation_count = 0;
        int z_max_update_count = 0;
        for (gsl::index j{0}; j < height; ++j) {
            // z_max contains the cutoffs for the j-th row.
            //std::vector<float> z_max(width, AZURE_KINECT_MAX_DISTANCE);
            std::vector<float> z_max_inv(width, 1.0 / AZURE_KINECT_MAX_DISTANCE);
            for (gsl::index i{width - 1}; i >= 0; --i) {
                // p stands for point.
                const gsl::index p_index{i + j * width};
                const int16_t z{depth_pixels[p_index]};

                // Skip invalid pixels.
                if (z == 0)
                    continue;

                // Invalidate the pixel if it is covered by another pixel.
                //if (z > z_max[i]) {
                if (z > (1.0f / z_max_inv[i])) {
                    depth_pixels[p_index] = 0;
                    ++invalidation_count;
                    continue;
                }

                //auto p{unit_depth_point_cloud_.points[p_index].xyz};
                //const float x{p.x};
                const float x{unit_depth_point_cloud_.points[p_index].xyz.x};
                const float z_inv{1.0f / z};

                for (gsl::index ii{i}; ii >= 0; --ii) {
                    //gsl::index pp_index{ii + j * width};
                    //auto pp{unit_depth_point_cloud_.points[pp_index].xyz};
                    //const float xx{pp.x};
                    //const float xx{unit_depth_point_cloud_.points[pp_index].xyz.x};
                    const float xx{unit_depth_point_cloud_.points[ii + j * width].xyz.x};
                    //const float zz{(color_camera_x_ * z) / ((xx - x) * z + color_camera_x_)};
                    //const float zz{1.0f / ((xx - x) * c_inv + z_inv)};
                    const float zz_inv{(xx - x) * c_inv + z_inv};

                    //if (zz >= z_max[ii])
                    if (zz_inv <= z_max_inv[ii])
                        break;

                    // If zz covers new area, update z_max that indicates the area covered
                    // and continue to the next pixels more on the right side.
                    z_max_inv[ii] = zz_inv;
                    ++z_max_update_count;
                }
            }
        }
        //std::cout << "invalidation_count: " << invalidation_count << ","
        //          << "z_max_update_count: " << z_max_update_count << "\n";
    }

private:
    PointCloud unit_depth_point_cloud_;
    float color_camera_x_;
};