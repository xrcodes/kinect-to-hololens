#include "kinect_video_sender.h"

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

void send_video_message(KinectVideoSenderResult result,
                        const tt::TimePoint& session_start_time,
                        k4a::calibration& calibration,
                        const int session_id,
                        UdpSocket& udp_socket,
                        VideoParityPacketStorage& video_parity_packet_storage,
                        std::unordered_map<int, RemoteReceiver>& remote_receivers,
                        std::mt19937& random_number_generator)
{
    // Create video/parity packet bytes.
    const float video_frame_time_stamp{(result.time_point - session_start_time).ms()};
    const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, result.keyframe, calibration, result.vp8_frame, result.trvl_frame, result.floor)};
    auto video_packet_bytes_set{split_video_sender_message_bytes(session_id, result.frame_id, message_bytes)};
    auto parity_packet_bytes_set{create_parity_sender_packet_bytes_set(session_id, result.frame_id, KH_FEC_PARITY_GROUP_SIZE, video_packet_bytes_set)};

    // Send video/parity packets.
    // Sending them in a random order makes the packets more robust to packet loss.
    std::vector<Bytes*> packet_bytes_ptrs;
    for (auto& video_packet_bytes : video_packet_bytes_set)
        packet_bytes_ptrs.push_back(&video_packet_bytes);

    for (auto& parity_packet_bytes : parity_packet_bytes_set)
        packet_bytes_ptrs.push_back(&parity_packet_bytes);

    std::shuffle(packet_bytes_ptrs.begin(), packet_bytes_ptrs.end(), random_number_generator);
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (!remote_receiver.video_requested)
            continue;

        for (auto& packet_bytes_ptr : packet_bytes_ptrs) {
            udp_socket.send(*packet_bytes_ptr, remote_receiver.endpoint);
        }
    }

    // Save video/parity packet bytes for retransmission. 
    video_parity_packet_storage.add(result.frame_id, std::move(video_packet_bytes_set), std::move(parity_packet_bytes_set));
}
}

// Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
KinectVideoSender::KinectVideoSender(const int session_id, KinectInterface& kinect_interface)
    : session_id_{session_id}
    , random_number_generator_{std::random_device{}()}
    , calibration_{kinect_interface.getCalibration()}
    , transformation_{calibration_}
    , color_encoder_{create_color_encoder(calibration_)}
    , depth_encoder_{create_depth_encoder(calibration_)}
    , occlusion_remover_{calibration_}
    , point_cloud_generator_{calibration_}
    , last_frame_id_{-1}
    , last_frame_time_{tt::TimePoint::now()}
{
}

void KinectVideoSender::send(const tt::TimePoint& session_start_time,
                             bool keyframe,
                             UdpSocket& udp_socket,
                             KinectInterface& kinect_interface,
                             VideoParityPacketStorage& video_parity_packet_storage,
                             std::unordered_map<int, RemoteReceiver>& remote_receivers,
                             KinectVideoSenderSummary& summary)
{
    // Try getting a Kinect frame.
    auto kinect_frame{kinect_interface.getFrame()};
    if (!kinect_frame) {
        std::cout << "no kinect frame...\n";
        return;
    }

    // Update last_frame_id_ and last_frame_time_ after testing all conditions.
    ++last_frame_id_;
    last_frame_time_ = kinect_frame->time_point;

    // Invalidate RGBD occluded depth pixels.
    auto occlusion_removal_start{tt::TimePoint::now()};
    gsl::span<int16_t> depth_image_span{reinterpret_cast<int16_t*>(kinect_frame->depth_image.get_buffer()),
                                        kinect_frame->depth_image.get_size()};

    occlusion_remover_.remove(depth_image_span);
    summary.occlusion_removal_ms_sum += occlusion_removal_start.elapsed_time().ms();

    // Map color pixels to depth pixels.
    auto transformation_start{tt::TimePoint::now()};
    const auto color_image_from_depth_camera{transformation_.color_image_to_depth_camera(kinect_frame->depth_image, kinect_frame->color_image)};
    summary.transformation_ms_sum += transformation_start.elapsed_time().ms();

    // Convert Kinect color pixels from BGRA to YUV420 for VP8.
    const auto yuv_conversion_start{tt::TimePoint::now()};
    const auto yuv_image{tt::createYuvFrameFromAzureKinectBgraBuffer(color_image_from_depth_camera.get_buffer(),
                                                                     color_image_from_depth_camera.get_width_pixels(),
                                                                     color_image_from_depth_camera.get_height_pixels(),
                                                                     color_image_from_depth_camera.get_stride_bytes())};
    summary.yuv_conversion_ms_sum += yuv_conversion_start.elapsed_time().ms();

    // VP8 compress color pixels.
    const auto color_encoder_start{tt::TimePoint::now()};
    const auto vp8_frame{color_encoder_.encode(yuv_image, keyframe)};
    summary.color_encoder_ms_sum += color_encoder_start.elapsed_time().ms();

    // TRVL compress depth pixels.
    const auto depth_encoder_start{tt::TimePoint::now()};
    const auto trvl_frame{depth_encoder_.encode(depth_image_span, keyframe)};
    summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

    // Try obtaining floor.
    const auto floor{detect_floor_plane_from_kinect_frame(point_cloud_generator_, *kinect_frame, calibration_)};

    // Updating variables for profiling.
    if (keyframe)
        ++summary.keyframe_count;
    ++summary.frame_count;
    summary.color_byte_count += gsl::narrow_cast<int>(vp8_frame.size());
    summary.depth_byte_count += gsl::narrow_cast<int>(trvl_frame.size());
    summary.frame_id = last_frame_id_;

    KinectVideoSenderResult result{last_frame_id_, kinect_frame->time_point, keyframe, vp8_frame, trvl_frame, floor};

    send_video_message(result, session_start_time, calibration_, session_id_, udp_socket, video_parity_packet_storage, remote_receivers, random_number_generator_);
}
}