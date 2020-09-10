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

class KinectVideoSender
{
public:
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    KinectVideoSender(const int session_id, KinectDeviceInterface& kinect_interface);
    int last_frame_id() { return last_frame_id_; }
    tt::TimePoint last_frame_time() { return last_frame_time_; }
    void send(const tt::TimePoint& session_start_time,
              bool keyframe,
              UdpSocket& udp_socket,
              KinectDeviceInterface& kinect_interface,
              VideoParityPacketStorage& video_parity_packet_storage,
              std::unordered_map<int, RemoteReceiver>& remote_receivers,
              KinectVideoSenderSummary& summary);
private:
    const int session_id_;
    std::mt19937 random_number_generator_;
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