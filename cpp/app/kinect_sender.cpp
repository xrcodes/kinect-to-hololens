#include <chrono>
#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "kh_opus.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "helper/kinect_helper.h"
#include "helper/soundio_helper.h"
#include "helper/shadow_remover.h"
#include <vector>
#include "sender/video_packet_sender.h"
#include "sender/audio_packet_sender.h"
#include "PointCloudGenerator.h"
#include "FloorDetector.h"


namespace kh
{
struct VideoReaderState
{
    int frame_id{0};
    TimePoint last_frame_time_point{TimePoint::now()};
};

struct ReadVideoFrameSummary
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
};

class MainKinectSender
{
public:
    static constexpr short CHANGE_THRESHOLD{10};
    static constexpr int INVALID_THRESHOLD{2};
    k4a::calibration calibration;
    k4a::transformation transformation;
    Vp8Encoder color_encoder;
    TrvlEncoder depth_encoder;
    ShadowRemover shadow_remover;
    Samples::PointCloudGenerator point_cloud_generator;

    MainKinectSender(KinectDevice& kinect_device)
        : calibration{kinect_device.getCalibration()}, transformation{calibration},
        color_encoder{calibration.depth_camera_calibration.resolution_width, calibration.depth_camera_calibration.resolution_height},
        depth_encoder{calibration.depth_camera_calibration.resolution_width * calibration.depth_camera_calibration.resolution_height, CHANGE_THRESHOLD, INVALID_THRESHOLD},
        shadow_remover{PointCloud::createUnitDepthPointCloud(calibration), calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0]},
        point_cloud_generator{calibration}
    {
    }

    void read(const int session_id,
              bool& stopped,
              UdpSocket& udp_socket,
              KinectDevice& kinect_device,
              moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
              ReceiverState& receiver_state)
    {
        TimePoint session_start_time{TimePoint::now()};

        //const k4a::calibration calibration{kinect_device.getCalibration()};
        //k4a::transformation transformation{calibration};

        //const int width{calibration.depth_camera_calibration.resolution_width};
        //const int height{calibration.depth_camera_calibration.resolution_height};

        // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
        //Vp8Encoder color_encoder{width, height};
        //TrvlEncoder depth_encoder{width * height, CHANGE_THRESHOLD, INVALID_THRESHOLD};

        //const float color_camera_x{calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0]};

        //ShadowRemover shadow_remover{ShadowRemover{PointCloud::createUnitDepthPointCloud(calibration), color_camera_x}};

        // frame_id is the ID of the frame the sender sends.
        VideoReaderState video_state;
        ReadVideoFrameSummary summary;

        //Samples::PointCloudGenerator point_cloud_generator{calibration};

        while (!stopped) {
            if (receiver_state.video_frame_id == ReceiverState::INITIAL_VIDEO_FRAME_ID) {
                const auto init_packet_bytes{create_init_sender_packet_bytes(session_id, create_init_sender_packet_data(calibration))};
                udp_socket.send(init_packet_bytes);

                Sleep(100);
            }

            auto kinect_frame{kinect_device.getFrame()};
            if (!kinect_frame) {
                std::cout << "no kinect frame...\n";
                continue;
            }

            point_cloud_generator.Update(kinect_frame->depth_image().handle());
            constexpr int downsampleStep{2};
            auto cloud_points{point_cloud_generator.GetCloudPoints(downsampleStep)};
            constexpr size_t minimumFloorPointCount{1024 / (downsampleStep * downsampleStep)};
            auto floor_plane{Samples::FloorDetector::TryDetectFloorPlane(cloud_points, kinect_frame->imu_sample(), calibration, minimumFloorPointCount)};

            if (!floor_plane)
                continue;

            const auto floor_packet_bytes{create_floor_sender_packet_bytes(session_id, floor_plane->Normal.X, 
                                                                           floor_plane->Normal.Y, floor_plane->Normal.Z,
                                                                           floor_plane->C)};
            udp_socket.send(floor_packet_bytes);

            constexpr float AZURE_KINECT_FRAME_RATE = 30.0f;
            const auto frame_time_point{TimePoint::now()};
            const auto frame_time_diff{frame_time_point - video_state.last_frame_time_point};
            const int frame_id_diff{video_state.frame_id - receiver_state.video_frame_id};
        
            if ((frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) < std::pow(2, frame_id_diff - 3))
                continue;

            video_state.last_frame_time_point = frame_time_point;

            const bool keyframe{frame_id_diff > 5};
        
            auto shadow_removal_start{TimePoint::now()};
            shadow_remover.remove({reinterpret_cast<int16_t*>(kinect_frame->depth_image().get_buffer()),
                                   gsl::narrow_cast<ptrdiff_t>(kinect_frame->depth_image().get_size())});
            summary.shadow_removal_ms_sum += shadow_removal_start.elapsed_time().ms();

            auto transformation_start{TimePoint::now()};
            const auto transformed_color_image{transformation.color_image_to_depth_camera(kinect_frame->depth_image(), kinect_frame->color_image())};
            summary.transformation_ms_sum += transformation_start.elapsed_time().ms();
        
            auto yuv_conversion_start{TimePoint::now()};
            // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
            const auto yuv_image{createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                         transformed_color_image.get_width_pixels(),
                                                                         transformed_color_image.get_height_pixels(),
                                                                         transformed_color_image.get_stride_bytes())};
            summary.yuv_conversion_ms_sum += yuv_conversion_start.elapsed_time().ms();

            auto color_encoder_start{TimePoint::now()};
            const auto vp8_frame{color_encoder.encode(yuv_image, keyframe)};
            summary.color_encoder_ms_sum += color_encoder_start.elapsed_time().ms();

            auto depth_encoder_start{TimePoint::now()};
            // Compress the depth pixels.
            const auto depth_encoder_frame{depth_encoder.encode({reinterpret_cast<const int16_t*>(kinect_frame->depth_image().get_buffer()),
                                                                 gsl::narrow_cast<ptrdiff_t>(kinect_frame->depth_image().get_size())},
                                                                keyframe)};
            summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

            const float video_frame_time_stamp{(frame_time_point - session_start_time).ms()};
            const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
            auto packet_bytes_set{split_video_sender_message_bytes(session_id, video_state.frame_id, message_bytes)};
            video_packet_queue.enqueue({video_state.frame_id, std::move(packet_bytes_set)});

            // Updating variables for profiling.
            if (keyframe)
                ++summary.keyframe_count;
            ++summary.frame_count;
            summary.byte_count += (vp8_frame.size() + depth_encoder_frame.size());

            const TimeDuration summary_duration{TimePoint::now() - summary.start_time};
            if (summary_duration.sec() > 10.0f) {
                std::cout << "Video Frame Summary:\n"
                          << "  Frame ID: " << video_state.frame_id << "\n"
                          << "  FPS: " << summary.frame_count / summary_duration.sec() << "\n"
                          << "  Bandwidth: " << summary.byte_count / summary_duration.sec() / (1024.0f * 1024.0f / 8.0f) << " Mbps\n"
                          << "  Keyframe Ratio: " << static_cast<float>(summary.keyframe_count) / summary.frame_count << " ms\n"
                          << "  Shadow Removal Time Average: " << summary.shadow_removal_ms_sum / summary.frame_count << " ms\n"
                          << "  Transformation Time Average: " << summary.transformation_ms_sum / summary.frame_count << " ms\n"
                          << "  Yuv Conversion Time Average: " << summary.yuv_conversion_ms_sum / summary.frame_count << " ms\n"
                          << "  Color Encoder Time Average: " << summary.color_encoder_ms_sum / summary.frame_count << " ms\n"
                          << "  Depth Encoder Time Average: " << summary.depth_encoder_ms_sum / summary.frame_count << " ms\n";
                summary = ReadVideoFrameSummary{};
            }
            ++video_state.frame_id;
        }
    }
};

void start(int port, int session_id, KinectDevice& kinect_device)
{
    constexpr int SENDER_SEND_BUFFER_SIZE = 128 * 1024;

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});

    std::vector<std::byte> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);
    if (error)
        throw std::runtime_error(std::string("Error receiving ping: ") + error.message());

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    UdpSocket udp_socket{std::move(socket), remote_endpoint};

    bool stopped{false};
    moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>> video_packet_queue;
    ReceiverState receiver_state;
    std::thread task_thread([&] {
        try {
            VideoPacketSender video_packet_sender;
            AudioPacketSender audio_packet_sender;
            
            VideoPacketSenderSummary video_packet_sender_summary;
            while (!stopped) {
                video_packet_sender.send(session_id, udp_socket, video_packet_queue, receiver_state, video_packet_sender_summary);
                audio_packet_sender.send(session_id, udp_socket);

                const TimeDuration summary_duration{TimePoint::now() - video_packet_sender_summary.start_time};
                if (summary_duration.sec() > 10.0f) {
                    std::cout << "Receiver Reported in " << video_packet_sender_summary.received_report_count / summary_duration.sec() << " Hz\n"
                        << "  Decoder Time Average: " << video_packet_sender_summary.decoder_time_ms_sum / video_packet_sender_summary.received_report_count << " ms\n"
                        << "  Frame Interval Time Average: " << video_packet_sender_summary.frame_interval_ms_sum / video_packet_sender_summary.received_report_count << " ms\n"
                        << "  Round Trip Time Average: " << video_packet_sender_summary.round_trip_ms_sum / video_packet_sender_summary.received_report_count << " ms\n";
                    video_packet_sender_summary = VideoPacketSenderSummary{};
                }
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError from task_thread:\n  " << e.what() << "\n";
        }
        stopped = true;
    });

    MainKinectSender main_kinect_sender{kinect_device};
    try {
        main_kinect_sender.read(session_id, stopped, udp_socket, kinect_device, video_packet_queue, receiver_state);
    } catch (UdpSocketRuntimeError e) {
        std::cout << "UdpSocketRuntimeError from send_video_frames(): \n  " << e.what() << "\n";
    }
    stopped = true;

    task_thread.join();
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void main()
{
    srand(time(nullptr));
    std::mt19937 rng{gsl::narrow_cast<unsigned int>(std::chrono::steady_clock::now().time_since_epoch().count())};

    for (;;) {
        std::string line;
        std::cout << "Enter a port number to start sending frames: ";
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        const int port{line.empty() ? 7777 : std::stoi(line)};
        
        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};
        
        KinectDevice kinect_device;
        kinect_device.start();

        start(port, session_id, kinect_device);
    }
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}
