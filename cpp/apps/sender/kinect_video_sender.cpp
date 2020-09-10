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

int get_minimum_receiver_frame_id(std::unordered_map<int, RemoteReceiver>& remote_receivers)
{
    int minimum_frame_id{INT_MAX};
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (remote_receiver.video_frame_id < minimum_frame_id)
            minimum_frame_id = remote_receiver.video_frame_id;
    }

    return minimum_frame_id;
}

std::pair<bool, bool> plan_frame(std::unordered_map<int, RemoteReceiver>& remote_receivers, int last_frame_id, tt::TimePoint last_frame_time)
{
    const int minimum_receiver_frame_id{get_minimum_receiver_frame_id(remote_receivers)};
    const bool has_new_receiver{minimum_receiver_frame_id == RemoteReceiver::INITIAL_VIDEO_FRAME_ID};

    constexpr float AZURE_KINECT_FRAME_RATE{30.0f};
    const auto frame_time_point{tt::TimePoint::now()};
    const auto frame_time_diff{frame_time_point - last_frame_time};
    const int frame_id_diff{last_frame_id - get_minimum_receiver_frame_id(remote_receivers)};

    // Skip a frame if there is no new receiver that requires a frame to start
    // and the sender is too much ahead of the receivers.
    const bool is_ready{has_new_receiver || ((frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) < std::pow(2, frame_id_diff - 1))};

    // Send a keyframe when there is a new receiver or at least a receiver needs to catch up by jumping forward using a keyframe.
    const bool keyframe{has_new_receiver || frame_id_diff > 5};

    return {is_ready, keyframe};
}

std::optional<Samples::Plane> detect_floor_plane_from_kinect_frame(Samples::PointCloudGenerator& point_cloud_generator,
                                                                   KinectFrame kinect_frame,
                                                                   k4a::calibration calibration)
{
    constexpr int DOWNSAMPLE_STEP{2};
    constexpr size_t MINIMUM_FLOOR_POINT_COUNT{1024 / (DOWNSAMPLE_STEP * DOWNSAMPLE_STEP)};

    point_cloud_generator.Update(kinect_frame.depth_image.handle());
    auto cloud_points{point_cloud_generator.GetCloudPoints(DOWNSAMPLE_STEP)};
    return Samples::FloorDetector::TryDetectFloorPlane(cloud_points, kinect_frame.imu_sample, calibration, MINIMUM_FLOOR_POINT_COUNT);
}
}

// Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
KinectVideoSender::KinectVideoSender(const int session_id, KinectDeviceInterface& kinect_interface)
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
                             UdpSocket& udp_socket,
                             KinectDeviceInterface& kinect_interface,
                             VideoParityPacketStorage& video_parity_packet_storage,
                             std::unordered_map<int, RemoteReceiver>& remote_receivers,
                             KinectVideoSenderSummary& summary)
{
    auto [is_ready, keyframe] = plan_frame(remote_receivers, last_frame_id_, last_frame_time_);
    if (!is_ready)
        return;

    // Try getting a Kinect frame.
    auto kinect_frame{kinect_interface.getFrame()};
    if (!kinect_frame) {
        std::cout << "no kinect frame...\n";
        return;
    }

    bool video_required_by_any = false;
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (remote_receiver.video_requested) {
            video_required_by_any = true;
            break;
        }
    }

    // Skip video compression if video is not required by any.
    if (!video_required_by_any)
        return;

    // Try obtaining floor.
    const auto floor_plane{detect_floor_plane_from_kinect_frame(point_cloud_generator_, *kinect_frame, calibration_)};
    std::optional<std::array<float, 4>> floor;
    if (floor_plane) {
        floor = std::array<float, 4>();
        floor->at(0) = floor_plane->Normal.X;
        floor->at(1) = floor_plane->Normal.Y;
        floor->at(2) = floor_plane->Normal.Z;
        floor->at(3) = floor_plane->C;
    } else {
        floor = std::nullopt;
    }

    // Update last_frame_id_ and last_frame_time_ after testing all conditions.
    ++last_frame_id_;
    last_frame_time_ = tt::TimePoint::now();

    // Remove the depth pixels that may not have corresponding color information available.
    auto occlusion_removal_start{tt::TimePoint::now()};
    gsl::span<int16_t> depth_image_span{reinterpret_cast<int16_t*>(kinect_frame->depth_image.get_buffer()),
                                        gsl::narrow_cast<size_t>(kinect_frame->depth_image.get_size())};
    occlusion_remover_.remove(depth_image_span);
    summary.occlusion_removal_ms_sum += occlusion_removal_start.elapsed_time().ms();

    // Transform the color image to match the depth image in a pixel by pixel manner.
    auto transformation_start{tt::TimePoint::now()};
    const auto color_image_from_depth_camera{transformation_.color_image_to_depth_camera(kinect_frame->depth_image, kinect_frame->color_image)};
    summary.transformation_ms_sum += transformation_start.elapsed_time().ms();

    // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
    const auto yuv_conversion_start{tt::TimePoint::now()};
    const auto yuv_image{tt::createYuvFrameFromAzureKinectBgraBuffer(color_image_from_depth_camera.get_buffer(),
                                                                     color_image_from_depth_camera.get_width_pixels(),
                                                                     color_image_from_depth_camera.get_height_pixels(),
                                                                     color_image_from_depth_camera.get_stride_bytes())};
    summary.yuv_conversion_ms_sum += yuv_conversion_start.elapsed_time().ms();

    // VP8 compress the color image.
    const auto color_encoder_start{tt::TimePoint::now()};
    const auto vp8_frame{color_encoder_.encode(yuv_image, keyframe)};
    summary.color_encoder_ms_sum += color_encoder_start.elapsed_time().ms();

    // TRVL compress the depth image.
    const auto depth_encoder_start{tt::TimePoint::now()};
    const auto depth_encoder_frame{depth_encoder_.encode(depth_image_span, keyframe)};
    summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

    // Create video/parity packet bytes.
    const float video_frame_time_stamp{(tt::TimePoint::now() - session_start_time).ms()};
    const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, keyframe, calibration_, vp8_frame, depth_encoder_frame, floor)};
    auto video_packet_bytes_set{split_video_sender_message_bytes(session_id_, last_frame_id_, message_bytes)};
    auto parity_packet_bytes_set{create_parity_sender_packet_bytes_set(session_id_, last_frame_id_, KH_FEC_PARITY_GROUP_SIZE, video_packet_bytes_set)};
    
    // Send video/parity packets.
    // Sending them in a random order makes the packets more robust to packet loss.
    std::vector<std::vector<std::byte>*> packet_bytes_ptrs;
    for (auto& video_packet_bytes : video_packet_bytes_set)
        packet_bytes_ptrs.push_back(&video_packet_bytes);
    
    for (auto& parity_packet_bytes : parity_packet_bytes_set)
        packet_bytes_ptrs.push_back(&parity_packet_bytes);

    std::shuffle(packet_bytes_ptrs.begin(), packet_bytes_ptrs.end(), random_number_generator_);
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (!remote_receiver.video_requested)
            continue;

        for (auto& packet_bytes_ptr : packet_bytes_ptrs) {
            udp_socket.send(*packet_bytes_ptr, remote_receiver.endpoint);
        }
    }

    // Save video/parity packet bytes for retransmission. 
    video_parity_packet_storage.add(last_frame_id_, std::move(video_packet_bytes_set), std::move(parity_packet_bytes_set));

    // Updating variables for profiling.
    if (keyframe)
        ++summary.keyframe_count;
    ++summary.frame_count;
    summary.color_byte_count += gsl::narrow_cast<int>(vp8_frame.size());
    summary.depth_byte_count += gsl::narrow_cast<int>(depth_encoder_frame.size());
    summary.frame_id = last_frame_id_;
}
}