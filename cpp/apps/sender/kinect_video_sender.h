#pragma once

#include <random>
#include "native/kh_native.h"
#include "video_sender_utils.h"

// These header files are from a Microsoft's Azure Kinect sample project.
#include "external/azure-kinect-samples/PointCloudGenerator.h"
#include "external/azure-kinect-samples/FloorDetector.h"

namespace kh
{
struct KinectVideoSenderSummary
{
    tt::TimePoint start_time{tt::TimePoint::now()};
    float occlusion_removal_ms_sum{0.0f};
    float transformation_ms_sum{0.0f};
    float yuv_conversion_ms_sum{0.0f};
    float color_encoder_ms_sum{0.0f};
    float depth_encoder_ms_sum{0.0f};
    int frame_count{0};
    int color_byte_count{0};
    int depth_byte_count{0};
    int keyframe_count{0};
    int frame_id{0};
};

//struct KinectVideoSenderResult
//{
//    int frame_id{0};
//    tt::TimePoint time_point{};
//    bool keyframe{false};
//    Bytes vp8_frame{};
//    Bytes trvl_frame{};
//    std::optional<std::array<float, 4>> floor;
//};

struct KinectVideoSenderResult
{
    int frame_id{0};
    tt::TimePoint time_point{};
    bool keyframe{false};
    Bytes vp8_frame{};
    Bytes trvl_frame{};
    std::optional<std::array<float, 4>> floor{};
};

class KinectVideoSender
{
public:
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    KinectVideoSender(k4a::calibration calibration);
    int last_frame_id() { return last_frame_id_; }
    tt::TimePoint last_frame_time() { return last_frame_time_; }
    std::optional<KinectVideoSenderResult> send(KinectInterface& kinect_interface,
                                                bool keyframe,
                                                KinectVideoSenderSummary& summary);
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