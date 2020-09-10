#include "video_pipeline.h"

#include <algorithm>
#include <iostream>

namespace kh
{
namespace
{
Vp8Encoder create_color_encoder(k4a::calibration calibration)
{
    return Vp8Encoder{calibration.depth_camera_calibration.resolution_width,
                      calibration.depth_camera_calibration.resolution_height};
}

tt::TrvlEncoder create_depth_encoder(k4a::calibration calibration)
{
    constexpr short CHANGE_THRESHOLD{10};
    //constexpr int INVALID_THRESHOLD{2};
    // Tolerating invalid pixels leaves black colored points left when combined with RGBD mapping.
    constexpr int INVALID_THRESHOLD{1};

    return tt::TrvlEncoder{calibration.depth_camera_calibration.resolution_width *
                           calibration.depth_camera_calibration.resolution_height,
                           CHANGE_THRESHOLD, INVALID_THRESHOLD};
}

std::optional<std::array<float, 4>> detect_floor_plane_from_kinect_frame(Samples::PointCloudGenerator& point_cloud_generator,
                                                                         KinectFrame kinect_frame,
                                                                         k4a::calibration calibration)
{
    constexpr int DOWNSAMPLE_STEP{2};
    constexpr size_t MINIMUM_FLOOR_POINT_COUNT{1024 / (DOWNSAMPLE_STEP * DOWNSAMPLE_STEP)};

    point_cloud_generator.Update(kinect_frame.depth_image.handle());
    auto cloud_points{point_cloud_generator.GetCloudPoints(DOWNSAMPLE_STEP)};
    auto floor_plane{Samples::FloorDetector::TryDetectFloorPlane(cloud_points, kinect_frame.imu_sample, calibration, MINIMUM_FLOOR_POINT_COUNT)};
    
    if (!floor_plane)
        return std::nullopt;

    return std::array<float, 4>{floor_plane->Normal.X, floor_plane->Normal.Y, floor_plane->Normal.Z, floor_plane->C};
}
}

// Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
VideoPipeline::VideoPipeline(k4a::calibration calibration)
    : calibration_{calibration}
    , transformation_{calibration}
    , color_encoder_{create_color_encoder(calibration)}
    , depth_encoder_{create_depth_encoder(calibration)}
    , occlusion_remover_{calibration_}
    , point_cloud_generator_{calibration_}
    , last_frame_id_{-1}
    , last_frame_time_{tt::TimePoint::now()}
{
}

VideoPipelineFrame VideoPipeline::process(KinectFrame& kinect_frame,
                                          bool keyframe,
                                          Profiler& profiler)
{
    gsl::span<int16_t> depth_image_span{reinterpret_cast<int16_t*>(kinect_frame.depth_image.get_buffer()),
                                        kinect_frame.depth_image.get_size()};

    // Update last_frame_id_ and last_frame_time_ after testing all conditions.
    ++last_frame_id_;
    last_frame_time_ = kinect_frame.time_point;

    // Invalidate RGBD occluded depth pixels.
    auto occlusion_removal_start{tt::TimePoint::now()};
    occlusion_remover_.remove(depth_image_span);
    profiler.addNumber("pipeline-occlusion", occlusion_removal_start.elapsed_time().ms());

    // Map color pixels to depth pixels.
    auto transformation_start{tt::TimePoint::now()};
    const auto color_image_from_depth_camera{transformation_.color_image_to_depth_camera(kinect_frame.depth_image, kinect_frame.color_image)};
    profiler.addNumber("pipeline-mapping", transformation_start.elapsed_time().ms());

    // Convert Kinect color pixels from BGRA to YUV420 for VP8.
    const auto yuv_conversion_start{tt::TimePoint::now()};
    const auto yuv_image{tt::createYuvFrameFromAzureKinectBgraBuffer(color_image_from_depth_camera.get_buffer(),
                                                                     color_image_from_depth_camera.get_width_pixels(),
                                                                     color_image_from_depth_camera.get_height_pixels(),
                                                                     color_image_from_depth_camera.get_stride_bytes())};
    profiler.addNumber("pipeline-yuv", yuv_conversion_start.elapsed_time().ms());

    // VP8 compress color pixels.
    const auto color_encoder_start{tt::TimePoint::now()};
    const auto vp8_frame{color_encoder_.encode(yuv_image, keyframe)};
    profiler.addNumber("pipeline-vp8", color_encoder_start.elapsed_time().ms());

    // TRVL compress depth pixels.
    const auto depth_encoder_start{tt::TimePoint::now()};
    const auto trvl_frame{depth_encoder_.encode(depth_image_span, keyframe)};
    profiler.addNumber("pipeline-trvl", depth_encoder_start.elapsed_time().ms());

    // Try obtaining floor.
    const auto floor_start{tt::TimePoint::now()};
    const auto floor{detect_floor_plane_from_kinect_frame(point_cloud_generator_, kinect_frame, calibration_)};
    profiler.addNumber("pipeline-floor", depth_encoder_start.elapsed_time().ms());

    // Updating variables for profiling.
    profiler.addNumber("pipeline-frame", 1);
    profiler.addNumber("pipeline-keyframe", keyframe ? 1 : 0);
    profiler.addNumber("pipeline-vp8byte", vp8_frame.size());
    profiler.addNumber("pipeline-trvlbyte", trvl_frame.size());

    return VideoPipelineFrame{last_frame_id_, kinect_frame.time_point, keyframe, vp8_frame, trvl_frame, floor};
}
}