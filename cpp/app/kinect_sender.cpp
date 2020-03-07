#include <chrono>
#include <iostream>
#include <random>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/kinect_helper.h"
#include "native/kh_udp_socket.h"
#include "kh_trvl.h"
#include "kh_vp8.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "helper/soundio_helper.h"
#include "kh_opus.h"

namespace kh
{
using Bytes = std::vector<std::byte>;

struct PointCloud
{
public:
    static PointCloud createUnitDepthPointCloud(const k4a::calibration& calibration)
    {
        PointCloud point_cloud;
        point_cloud.width = calibration.depth_camera_calibration.resolution_width;
        point_cloud.height = calibration.depth_camera_calibration.resolution_height;

        point_cloud.points.resize(point_cloud.width * point_cloud.height);
        
        for (gsl::index j{0}; j < point_cloud.height; ++j) {
            for (gsl::index i{0}; i < point_cloud.width; ++i) {
                if (!calibration.convert_2d_to_3d(k4a_float2_t{gsl::narrow_cast<float>(i), gsl::narrow_cast<float>(j)},
                                                  1.0f,
                                                  K4A_CALIBRATION_TYPE_DEPTH,
                                                  K4A_CALIBRATION_TYPE_DEPTH,
                                                  &point_cloud.points[i + j * point_cloud.width])) {
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
        const int width{unit_depth_point_cloud_.width};
        const int height{unit_depth_point_cloud_.height};
        for (gsl::index j{0}; j < height; ++j) {

            // 3.86 m is the operating range of NFOV unbinned mode of Azure Kinect.
            std::vector<float> z_max(width, 3860.0f);
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

struct ReceiverState
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};
    int video_frame_id{INITIAL_VIDEO_FRAME_ID};
};

struct HandleReceiverMessageSummary
{
    TimePoint start_time{TimePoint::now()};
    int received_report_count{0};
};

struct HandleReceiverMessageTask
{
    constexpr static int XOR_MAX_GROUP_SIZE = 5;
    std::unordered_map<int, std::pair<int, std::vector<Bytes>>> video_packet_sets;
    std::unordered_map<int, TimePoint> video_frame_send_times;
    HandleReceiverMessageSummary summary;

    HandleReceiverMessageTask()
    {
    }

    void run(const int session_id,
             UdpSocket& sender_socket,
             moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
             ReceiverState& receiver_state)
    {
        for (;;) {
            std::optional<std::vector<std::byte>> received_packet{sender_socket.receive()};
            if (!received_packet)
                break;

            const auto message_type{get_packet_type_from_receiver_packet_bytes(*received_packet)};

            if (message_type == ReceiverPacketType::Report) {
                const auto report_receiver_packet_data{parse_report_receiver_packet_bytes(*received_packet)};
                receiver_state.video_frame_id = report_receiver_packet_data.frame_id;

                const auto round_trip_time{TimePoint::now() - video_frame_send_times[receiver_state.video_frame_id]};

                //std::cout << "Receiver Report - frame id: " << report_receiver_packet_data.frame_id << ", "
                //          << "decoding: " << report_receiver_packet_data.decoder_time_ms << " ms, "
                //          << "between-frame: " << report_receiver_packet_data.frame_time_ms << " ms, "
                //          << "round trip: " << round_trip_time.ms() << " ms\n";

                ++summary.received_report_count;
            } else if (message_type == ReceiverPacketType::Request) {
                const auto request_receiver_packet_data{parse_request_receiver_packet_bytes(*received_packet)};

                for (int packet_index : request_receiver_packet_data.packet_indices) {
                    if (video_packet_sets.find(request_receiver_packet_data.frame_id) == video_packet_sets.end())
                        continue;

                    sender_socket.send(video_packet_sets[request_receiver_packet_data.frame_id].second[packet_index]);
                }
            }
        }

        std::pair<int, std::vector<Bytes>> video_packet_set;
        while (video_packet_queue.try_dequeue(video_packet_set)) {
            auto fec_packet_bytes_set{create_fec_sender_packet_bytes_set(session_id, video_packet_set.first, XOR_MAX_GROUP_SIZE, video_packet_set.second)};

            video_frame_send_times.insert({video_packet_set.first, TimePoint::now()});
            for (auto& packet : video_packet_set.second) {
                std::error_code error;
                sender_socket.send(packet);
            }

            for (auto& fec_packet_bytes : fec_packet_bytes_set) {
                std::error_code error;
                sender_socket.send(fec_packet_bytes);
            }
            video_packet_sets.insert({video_packet_set.first, std::move(video_packet_set)});
        }

        // Remove elements of frame_packet_sets reserved for filling up missing packets
        // if they are already used from the receiver side.
        for (auto it = video_packet_sets.begin(); it != video_packet_sets.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_packet_sets.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = video_frame_send_times.begin(); it != video_frame_send_times.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_frame_send_times.erase(it);
            } else {
                ++it;
            }
        }

        const TimeDuration summary_duration{TimePoint::now() - summary.start_time};
        if (summary_duration.sec() > 10.0f) {
            std::cout << "Receiver reported "
                << summary.received_report_count / summary_duration.sec()
                << " times per second\n";
            summary = HandleReceiverMessageSummary{};
        }
    }
};

struct AudioSenderState
{
    int frame_id{0};
};

struct SendAudioFrameSummary
{
    TimePoint start_time{TimePoint::now()};
    int sent_byte_count{0};
};

struct SendAudioFrameTask
{
    Audio audio;
    AudioInStream kinect_microphone_stream{create_kinect_microphone_stream(audio)};
    AudioEncoder audio_encoder{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false};

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm;
    AudioSenderState audio_state;
    SendAudioFrameSummary summary;

    SendAudioFrameTask()
    {
        constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::exception("Failed in soundio_ring_buffer_create()...");

        kinect_microphone_stream.start();
    }

    void run(int session_id, UdpSocket& udp_socket)
    {
        soundio_flush_events(audio.get());
        char* read_ptr = soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer);
        int fill_bytes = soundio_ring_buffer_fill_count(soundio_callback::ring_buffer);

        const int BYTES_PER_FRAME{gsl::narrow_cast<int>(sizeof(float) * pcm.size())};
        int cursor = 0;
        while ((fill_bytes - cursor) > BYTES_PER_FRAME) {
            memcpy(pcm.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder.encode(opus_frame.data(), pcm.data(), KH_SAMPLES_PER_FRAME, opus_frame.size());
            opus_frame.resize(opus_frame_size);

            std::error_code error;
            udp_socket.send(create_audio_sender_packet_bytes(session_id, audio_state.frame_id++, opus_frame));

            cursor += BYTES_PER_FRAME;
            summary.sent_byte_count += opus_frame.size();
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);

        const TimeDuration summary_duration{TimePoint::now() - summary.start_time};
        if (summary_duration.sec() > 10.0f) {
            std::cout << "SendAudioFrameSummary - Bandwidth: "
                << summary.sent_byte_count / (1024.0f * 1024.0f / 8.0f) / summary_duration.sec()
                << " Mbps\n";
            summary = SendAudioFrameSummary{};
        }
    }
};

struct VideoSenderState
{
    int frame_id{0};
};

struct SendVideoFrameSummary
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

void send_video_frames(const int session_id,
                       bool& stopped,
                       UdpSocket& udp_socket,
                       KinectDevice& kinect_device,
                       moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
                       ReceiverState& receiver_state)
{
    constexpr short CHANGE_THRESHOLD = 10;
    constexpr int INVALID_THRESHOLD = 2;

    const auto calibration{kinect_device.getCalibration()};
    k4a::transformation transformation{calibration};

    const int width{calibration.depth_camera_calibration.resolution_width};
    const int height{calibration.depth_camera_calibration.resolution_height};

    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    Vp8Encoder color_encoder{width, height};
    TrvlEncoder depth_encoder{width * height, CHANGE_THRESHOLD, INVALID_THRESHOLD};

    const float color_camera_x{calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH].translation[0]};

    auto shadow_remover{ShadowRemover{PointCloud::createUnitDepthPointCloud(calibration), color_camera_x}};

    // frame_id is the ID of the frame the sender sends.
    VideoSenderState video_state;

    // Variables for profiling the sender.
    auto last_device_time_stamp{std::chrono::microseconds::zero()};

    SendVideoFrameSummary summary;
    while (!stopped) {
        if (receiver_state.video_frame_id == ReceiverState::INITIAL_VIDEO_FRAME_ID) {
            const auto init_packet_bytes{create_init_sender_packet_bytes(session_id, create_init_sender_packet_data(calibration))};
            std::error_code error;
            udp_socket.send(init_packet_bytes);

            Sleep(100);
        }

        const auto capture{kinect_device.getCapture()};
        if (!capture) {
            std::cout << "no capture...\n";
            continue;
        }

        const auto color_image{capture->get_color_image()};
        if (!color_image) {
            std::cout << "get_color_image() failed...\n";
            continue;
        }

        auto depth_image{capture->get_depth_image()};
        if (!depth_image) {
            std::cout << "get_depth_image() failed...\n";
            continue;
        }

        const auto device_time_stamp{color_image.get_device_timestamp()};
        const auto device_time_diff{device_time_stamp - last_device_time_stamp};
        const int device_frame_diff{gsl::narrow_cast<int>(device_time_diff.count() / 33000.0f + 0.5f)};
        const int frame_id_diff{video_state.frame_id - receiver_state.video_frame_id};

        if (device_frame_diff < static_cast<int>(std::pow(2, frame_id_diff - 3)))
            continue;

        last_device_time_stamp = device_time_stamp;

        const bool keyframe{frame_id_diff > 5};
        
        auto shadow_removal_start{TimePoint::now()};
        shadow_remover.remove({reinterpret_cast<int16_t*>(depth_image.get_buffer()),
                               gsl::narrow_cast<ptrdiff_t>(depth_image.get_size())});
        summary.shadow_removal_ms_sum += shadow_removal_start.elapsed_time().ms();

        auto transformation_start{TimePoint::now()};
        const auto transformed_color_image{transformation.color_image_to_depth_camera(depth_image, color_image)};
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
        //const auto depth_encoder_frame{depth_encoder.encode(reinterpret_cast<const int16_t*>(depth_image.get_buffer()), keyframe)};
        const auto depth_encoder_frame{depth_encoder.encode({reinterpret_cast<const int16_t*>(depth_image.get_buffer()),
                                                             gsl::narrow_cast<ptrdiff_t>(depth_image.get_size())},
                                                            keyframe)};
        summary.depth_encoder_ms_sum += depth_encoder_start.elapsed_time().ms();

        const float frame_time_stamp{device_time_stamp.count() / 1000.0f};
        const auto message_bytes{create_video_sender_message_bytes(frame_time_stamp, keyframe, vp8_frame, depth_encoder_frame)};
        auto packet_bytes_set{split_video_sender_message_bytes(session_id, video_state.frame_id, message_bytes)};
        video_packet_queue.enqueue({video_state.frame_id, std::move(packet_bytes_set)});

        // Updating variables for profiling.
        if (keyframe)
            ++summary.keyframe_count;
        ++summary.frame_count;
        summary.byte_count += (vp8_frame.size() + depth_encoder_frame.size());

        const TimeDuration summary_duration{TimePoint::now() - summary.start_time};
        if (summary_duration.sec() > 10.0f) {
            std::cout << "Video Frame Summary - "
                      << "id: " << video_state.frame_id << ", "
                      << "FPS: " << summary.frame_count / summary_duration.sec() << ", "
                      << "Bandwidth: " << summary.byte_count / summary_duration.sec() << ", "
                      << "Keyframe Ratio: " << static_cast<float>(summary.keyframe_count) / summary.frame_count << ", "
                      << "Shadow Removal: " << summary.shadow_removal_ms_sum / summary.frame_count << ", "
                      << "Transformation: " << summary.transformation_ms_sum / summary.frame_count << ", "
                      << "Yuv Conversion: " << summary.yuv_conversion_ms_sum / summary.frame_count << ", "
                      << "Color Encoder: " << summary.color_encoder_ms_sum / summary.frame_count << ", "
                      << "Depth Encoder " << summary.depth_encoder_ms_sum / summary.frame_count << "\n";
            summary = SendVideoFrameSummary{};
        }
        ++video_state.frame_id;
    }
}

void send_frames(int port, int session_id, KinectDevice& kinect_device)
{
    constexpr int SENDER_SEND_BUFFER_SIZE = 128 * 1024;

    printf("Start Sending Frames (session_id: %d, port: %d)\n", session_id, port);

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
            HandleReceiverMessageTask handle_receiver_message_task;
            SendAudioFrameTask send_audio_frame_task;
            while (!stopped) {
                handle_receiver_message_task.run(session_id, udp_socket, video_packet_queue, receiver_state);
                send_audio_frame_task.run(session_id, udp_socket);
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError from task_thread:\n  " << e.what() << "\n";
        }
        stopped = true;
    });

    try {
        send_video_frames(session_id, stopped, udp_socket, kinect_device, video_packet_queue, receiver_state);
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
        
        k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
        configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
        configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
        configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        constexpr auto timeout = std::chrono::milliseconds(1000);

        const int session_id{gsl::narrow_cast<const int>(rng() % (static_cast<unsigned int>(INT_MAX) + 1))};
        
        KinectDevice kinect_device{configuration, timeout};
        kinect_device.start();

        send_frames(port, session_id, kinect_device);
    }
}
}

int main()
{
    //std::ios_base::sync_with_stdio(false);
    kh::main();
    return 0;
}