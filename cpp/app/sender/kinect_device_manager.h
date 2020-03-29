#pragma once

namespace kh
{
struct VideoReaderState
{
    int frame_id{0};
    TimePoint last_frame_time_point{TimePoint::now()};
};

struct KinectDeviceManagerSummary
{
    TimePoint start_time{TimePoint::now()};
    float shadow_removal_ms_sum{0.0f};
    float transformation_ms_sum{0.0f};
    float yuv_conversion_ms_sum{0.0f};
    float color_encoder_ms_sum{0.0f};
    float depth_encoder_ms_sum{0.0f};
    int frame_count{0};
    int byte_count{0};
    int keyframe_count{0};
    int frame_id{0};
};

class KinectDeviceManager
{
public:
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    KinectDeviceManager(KinectDevice&& kinect_device)
        : kinect_device_{std::move(kinect_device)}, calibration_{kinect_device_.getCalibration()}, transformation_{calibration_},
        color_encoder_{calibration_.depth_camera_calibration.resolution_width,
                      calibration_.depth_camera_calibration.resolution_height},
        depth_encoder_{calibration_.depth_camera_calibration.resolution_width *
                      calibration_.depth_camera_calibration.resolution_height,
                      CHANGE_THRESHOLD, INVALID_THRESHOLD},
        shadow_remover_{PointCloud::createUnitDepthPointCloud(calibration_), calibration_.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0]},
        point_cloud_generator_{calibration_}, video_state_{}
    {
    }

    void update(const int session_id,
                const TimePoint& session_start_time,
                bool& stopped,
                UdpSocket& udp_socket,
                moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
                ReceiverState& receiver_state,
                KinectDeviceManagerSummary& summary)
    {
        if (receiver_state.video_frame_id == ReceiverState::INITIAL_VIDEO_FRAME_ID) {
            const auto init_packet_bytes{create_init_sender_packet_bytes(session_id, create_init_sender_packet_data(calibration_))};
            udp_socket.send(init_packet_bytes);

            Sleep(100);
        }

        auto kinect_frame{kinect_device_.getFrame()};
        if (!kinect_frame) {
            std::cout << "no kinect frame...\n";
            //continue;
            return;
        }

        point_cloud_generator_.Update(kinect_frame->depth_image().handle());
        constexpr int downsampleStep{2};
        auto cloud_points{point_cloud_generator_.GetCloudPoints(downsampleStep)};
        constexpr size_t minimumFloorPointCount{1024 / (downsampleStep * downsampleStep)};
        auto floor_plane{Samples::FloorDetector::TryDetectFloorPlane(cloud_points, kinect_frame->imu_sample(), calibration_, minimumFloorPointCount)};

        if (!floor_plane)
            return;

        const auto floor_packet_bytes{create_floor_sender_packet_bytes(session_id, floor_plane->Normal.X,
                                                                        floor_plane->Normal.Y, floor_plane->Normal.Z,
                                                                        floor_plane->C)};
        udp_socket.send(floor_packet_bytes);

        constexpr float AZURE_KINECT_FRAME_RATE = 30.0f;
        const auto frame_time_point{TimePoint::now()};
        const auto frame_time_diff{frame_time_point - video_state_.last_frame_time_point};
        const int frame_id_diff{video_state_.frame_id - receiver_state.video_frame_id};

        if ((frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) < std::pow(2, frame_id_diff - 3))
            return;

        video_state_.last_frame_time_point = frame_time_point;

        const bool keyframe{frame_id_diff > 5};

        auto shadow_removal_start{TimePoint::now()};
        shadow_remover_.remove({reinterpret_cast<int16_t*>(kinect_frame->depth_image().get_buffer()),
                                gsl::narrow_cast<ptrdiff_t>(kinect_frame->depth_image().get_size())});
        summary.shadow_removal_ms_sum += shadow_removal_start.elapsed_time().ms();

        auto transformation_start{TimePoint::now()};
        const auto transformed_color_image{transformation_.color_image_to_depth_camera(kinect_frame->depth_image(), kinect_frame->color_image())};
        summary.transformation_ms_sum += transformation_start.elapsed_time().ms();

        auto yuv_conversion_start{TimePoint::now()};
        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        const auto yuv_image{createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                        transformed_color_image.get_width_pixels(),
                                                                        transformed_color_image.get_height_pixels(),
                                                                        transformed_color_image.get_stride_bytes())};
        summary.yuv_conversion_ms_sum += yuv_conversion_start.elapsed_time().ms();

        auto color_encoder_start{TimePoint::now()};
        const auto vp8_frame{color_encoder_.encode(yuv_image, keyframe)};
        summary.color_encoder_ms_sum += color_encoder_start.elapsed_time().ms();

        auto depth_encoder_start{TimePoint::now()};
        // Compress the depth pixels.
        const auto depth_encoder_frame{depth_encoder_.encode({reinterpret_cast<const int16_t*>(kinect_frame->depth_image().get_buffer()),
                                                                gsl::narrow_cast<ptrdiff_t>(kinect_frame->depth_image().get_size())},
                                                            keyframe)};
        summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

        const float video_frame_time_stamp{(frame_time_point - session_start_time).ms()};
        const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
        auto packet_bytes_set{split_video_sender_message_bytes(session_id, video_state_.frame_id, message_bytes)};
        video_packet_queue.enqueue({video_state_.frame_id, std::move(packet_bytes_set)});

        // Updating variables for profiling.
        if (keyframe)
            ++summary.keyframe_count;
        ++summary.frame_count;
        summary.byte_count += (vp8_frame.size() + depth_encoder_frame.size());
        summary.frame_id = video_state_.frame_id;

        ++video_state_.frame_id;
    }
private:
    static constexpr short CHANGE_THRESHOLD{10};
    static constexpr int INVALID_THRESHOLD{2};
    KinectDevice kinect_device_;
    k4a::calibration calibration_;
    k4a::transformation transformation_;
    Vp8Encoder color_encoder_;
    TrvlEncoder depth_encoder_;
    ShadowRemover shadow_remover_;
    Samples::PointCloudGenerator point_cloud_generator_;
    VideoReaderState video_state_;
};
}