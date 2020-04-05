#include "kinect_device_manager.h"

#include "video_sender_utils.h"

namespace kh
{
namespace
{
Vp8Encoder create_color_encoder(k4a::calibration calibration)
{
    return Vp8Encoder{calibration.depth_camera_calibration.resolution_width,
                      calibration.depth_camera_calibration.resolution_height};
}

TrvlEncoder create_depth_encoder(k4a::calibration calibration)
{
    constexpr short CHANGE_THRESHOLD{10};
    constexpr int INVALID_THRESHOLD{2};

    return TrvlEncoder{calibration.depth_camera_calibration.resolution_width *
                       calibration.depth_camera_calibration.resolution_height,
                       CHANGE_THRESHOLD, INVALID_THRESHOLD};
}

float get_color_camera_x_from_calibration(k4a::calibration calibration)
{
    return calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0];
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
KinectDeviceManager::KinectDeviceManager(const int session_id, const asio::ip::udp::endpoint remote_endpoint, KinectDevice&& kinect_device)
    : session_id_{session_id}, remote_endpoint_{remote_endpoint}, kinect_device_{std::move(kinect_device)},
    calibration_{kinect_device_.getCalibration()}, transformation_{calibration_},
    color_encoder_{create_color_encoder(calibration_)}, depth_encoder_{create_depth_encoder(calibration_)},
    shadow_remover_{PointCloud::createUnitDepthPointCloud(calibration_), get_color_camera_x_from_calibration(calibration_)},
    point_cloud_generator_{calibration_}, state_{}
{
}

void KinectDeviceManager::update(const TimePoint& session_start_time,
                                 UdpSocket& udp_socket,
                                 VideoFecPacketStorage& video_fec_packet_storage,
                                 ReceiverState& receiver_state,
                                 KinectDeviceManagerSummary& summary)
{
    // Keep send the init packet until receiver reports a received frame.
    if (receiver_state.video_frame_id == ReceiverState::INITIAL_VIDEO_FRAME_ID) {
        const auto init_packet_bytes{create_init_sender_packet_bytes(session_id_, create_init_sender_packet_data(calibration_))};
        udp_socket.send(init_packet_bytes, remote_endpoint_);

        Sleep(100);
    }

    // Try getting a Kinect frame.
    auto kinect_frame{kinect_device_.getFrame()};
    if (!kinect_frame) {
        std::cout << "no kinect frame...\n";
        return;
    }

    constexpr float AZURE_KINECT_FRAME_RATE = 30.0f;
    const auto frame_time_point{TimePoint::now()};
    const auto frame_time_diff{frame_time_point - state_.last_frame_time_point};
    const int frame_id_diff{state_.frame_id - receiver_state.video_frame_id};

    if ((frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) < std::pow(2, frame_id_diff - 3))
        return;

    // Try sending the floor plane from the Kinect frame.
    auto floor_plane{detect_floor_plane_from_kinect_frame(point_cloud_generator_, *kinect_frame, calibration_)};
    if (floor_plane) {
        const auto floor_packet_bytes{create_floor_sender_packet_bytes(session_id_,
                                                                       floor_plane->Normal.X,
                                                                       floor_plane->Normal.Y,
                                                                       floor_plane->Normal.Z,
                                                                       floor_plane->C)};
        udp_socket.send(floor_packet_bytes, remote_endpoint_);
    }

    state_.last_frame_time_point = frame_time_point;

    const bool keyframe{frame_id_diff > 5};

    // Remove the depth pixels that may not have corresponding color information available.
    auto shadow_removal_start{TimePoint::now()};
    gsl::span<int16_t> depth_image_span{reinterpret_cast<int16_t*>(kinect_frame->depth_image.get_buffer()),
                                        gsl::narrow_cast<ptrdiff_t>(kinect_frame->depth_image.get_size())};
    shadow_remover_.remove(depth_image_span);
    summary.shadow_removal_ms_sum += shadow_removal_start.elapsed_time().ms();

    // Transform the color image to match the depth image in a pixel by pixel manner.
    auto transformation_start{TimePoint::now()};
    const auto color_image_from_depth_camera{transformation_.color_image_to_depth_camera(kinect_frame->depth_image, kinect_frame->color_image)};
    summary.transformation_ms_sum += transformation_start.elapsed_time().ms();

    // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
    const auto yuv_conversion_start{TimePoint::now()};
    const auto yuv_image{createYuvImageFromAzureKinectBgraBuffer(color_image_from_depth_camera.get_buffer(),
                                                                 color_image_from_depth_camera.get_width_pixels(),
                                                                 color_image_from_depth_camera.get_height_pixels(),
                                                                 color_image_from_depth_camera.get_stride_bytes())};
    summary.yuv_conversion_ms_sum += yuv_conversion_start.elapsed_time().ms();

    // VP8 compress the color image.
    const auto color_encoder_start{TimePoint::now()};
    const auto vp8_frame{color_encoder_.encode(yuv_image, keyframe)};
    summary.color_encoder_ms_sum += color_encoder_start.elapsed_time().ms();

    // TRVL compress the depth image.
    const auto depth_encoder_start{TimePoint::now()};
    const auto depth_encoder_frame{depth_encoder_.encode(depth_image_span, keyframe)};
    summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

    // Create video/fec packet bytes.
    const float video_frame_time_stamp{(frame_time_point - session_start_time).ms()};
    const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
    auto video_packet_bytes_set{split_video_sender_message_bytes(session_id_, state_.frame_id, message_bytes)};
    auto fec_packet_bytes_set{create_fec_sender_packet_bytes_set(session_id_, state_.frame_id, XOR_MAX_GROUP_SIZE, video_packet_bytes_set)};
    
    // Send video/fec packets.
    for (auto& packet : video_packet_bytes_set) {
        udp_socket.send(packet, remote_endpoint_);
    }

    for (auto& fec_packet_bytes : fec_packet_bytes_set) {
        udp_socket.send(fec_packet_bytes, remote_endpoint_);
    }

    // Save video/fec packet bytes for retransmission. 
    video_fec_packet_storage.add(state_.frame_id, std::move(video_packet_bytes_set), std::move(fec_packet_bytes_set));

    // Updating variables for profiling.
    if (keyframe)
        ++summary.keyframe_count;
    ++summary.frame_count;
    summary.byte_count += gsl::narrow_cast<int>(vp8_frame.size() + depth_encoder_frame.size());
    summary.frame_id = state_.frame_id;

    ++state_.frame_id;
}
}