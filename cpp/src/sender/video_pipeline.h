#pragma once

#include "native/kh_native.h"
#include "native/profiler.h"
#include "occlusion_remover.h"
#include "win32/kh_kinect.h"

// These header files are from a Microsoft's Azure Kinect sample project.
#include "external/PointCloudGenerator.h"
#include "external/FloorDetector.h"

namespace kh
{
struct VideoPipelineFrame
{
    int frame_id{0};
    tt::TimePoint time_point{};
    bool keyframe{false};
    std::vector<std::byte> vp8_frame{};
    std::vector<std::byte> trvl_frame{};
    std::optional<std::array<float, 4>> floor{};
};

class VideoPipeline
{
public:
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    VideoPipeline(k4a::calibration calibration);
    int last_frame_id() { return last_frame_id_; }
    tt::TimePoint last_frame_time() { return last_frame_time_; }
    VideoPipelineFrame process(KinectFrame& kinect_frame,
                               bool keyframe,
                               Profiler& profiler);
private:
    k4a::calibration calibration_;
    k4a::transformation transformation_;
    Vp8Encoder color_encoder_;
    tt::TrvlEncoder depth_encoder_;
    OcclusionRemover occlusion_remover_;
    Samples::PointCloudGenerator point_cloud_generator_;
    int last_frame_id_;
    tt::TimePoint last_frame_time_;
};
}