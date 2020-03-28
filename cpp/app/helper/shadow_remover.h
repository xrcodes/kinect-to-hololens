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

    void remove(gsl::span<int16_t> depth_pixels)
    {
        constexpr float AZURE_KINECT_MAX_DISTANCE{3860.0f};
        const int width{unit_depth_point_cloud_.width};
        const int height{unit_depth_point_cloud_.height};
        for (gsl::index j{0}; j < height; ++j) {

            // 3.86 m is the operating range of NFOV unbinned mode of Azure Kinect.
            std::vector<float> z_max(width, AZURE_KINECT_MAX_DISTANCE);
            for (gsl::index i{width - 1}; i >= 0; --i) {
                // p stands for point.
                const gsl::index p_index{i + j * width};
                const int16_t z{depth_pixels[p_index]};

                // Shadow removal has nothing to do with already invalid pixels.
                if (depth_pixels[p_index] == 0)
                    continue;

                // Zero and skip the pixel if it is covered by another pixel.
                if (depth_pixels[p_index] > z_max[i]) {
                    depth_pixels[p_index] = 0;
                    continue;
                }

                //auto p{unit_depth_point_cloud_.points[p_index].xyz};
                //const float x{p.x};
                const float x{unit_depth_point_cloud_.points[p_index].xyz.x};

                for (gsl::index ii{i}; ii >= 0; --ii) {
                    //gsl::index pp_index{ii + j * width};
                    //auto pp{unit_depth_point_cloud_.points[pp_index].xyz};
                    //const float xx{pp.x};
                    //const float xx{unit_depth_point_cloud_.points[pp_index].xyz.x};
                    const float xx{unit_depth_point_cloud_.points[ii + j * width].xyz.x};
                    const float zz{(color_camera_x_ * z) / ((xx - x) * z + color_camera_x_)};

                    if (zz >= z_max[ii])
                        break;

                    // If zz covers new area, update z_max that indicates the area covered
                    // and continue to the next pixels more on the right side.
                    z_max[ii] = zz;
                }
            }
        }
    }

private:
    PointCloud unit_depth_point_cloud_;
    float color_camera_x_;
};