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

namespace kh
{
using Bytes = std::vector<std::byte>;

struct ReceiverState
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};
    int video_frame_id{INITIAL_VIDEO_FRAME_ID};
};

struct HandleReceiverMessageSummary
{
    TimePoint start_time{TimePoint::now()};
    float decoder_time_ms_sum{0.0f};
    float frame_interval_ms_sum{0.0f};
    float round_trip_ms_sum{0.0f};
    int received_report_count{0};
};

class VideoPacketSender
{
public:
    void send(const int session_id,
              UdpSocket& udp_socket,
              moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
              ReceiverState& receiver_state)
    {
        while (auto received_packet{udp_socket.receive()}) {
            switch (get_packet_type_from_receiver_packet_bytes(*received_packet)) {
            case ReceiverPacketType::Report: {
                    const auto report_receiver_packet_data{parse_report_receiver_packet_bytes(*received_packet)};
                    receiver_state.video_frame_id = report_receiver_packet_data.frame_id;

                    const auto round_trip_time{TimePoint::now() - video_frame_send_times[receiver_state.video_frame_id]};
                    summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
                    summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
                    summary.round_trip_ms_sum += round_trip_time.ms();
                    ++summary.received_report_count;
                }
                break;
            case ReceiverPacketType::Request: {
                    const auto request_receiver_packet_data{parse_request_receiver_packet_bytes(*received_packet)};

                    for (int packet_index : request_receiver_packet_data.packet_indices) {
                        if (video_packet_sets.find(request_receiver_packet_data.frame_id) == video_packet_sets.end())
                            continue;

                        udp_socket.send(video_packet_sets[request_receiver_packet_data.frame_id].second[packet_index]);
                    }
                }
                break;
            }
        }

        std::pair<int, std::vector<Bytes>> video_packet_set;
        while (video_packet_queue.try_dequeue(video_packet_set)) {
            auto fec_packet_bytes_set{create_fec_sender_packet_bytes_set(session_id, video_packet_set.first, XOR_MAX_GROUP_SIZE, video_packet_set.second)};

            video_frame_send_times.insert({video_packet_set.first, TimePoint::now()});
            for (auto& packet : video_packet_set.second) {
                std::error_code error;
                udp_socket.send(packet);
            }

            for (auto& fec_packet_bytes : fec_packet_bytes_set) {
                std::error_code error;
                udp_socket.send(fec_packet_bytes);
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
            std::cout << "Receiver Reported in "<< summary.received_report_count / summary_duration.sec() << " Hz\n"
                      << "  Decoder Time Average: " << summary.decoder_time_ms_sum / summary.received_report_count << " ms\n"
                      << "  Frame Interval Time Average: " << summary.frame_interval_ms_sum / summary.received_report_count << " ms\n"
                      << "  Round Trip Time Average: " << summary.round_trip_ms_sum / summary.received_report_count << " ms\n";
            summary = HandleReceiverMessageSummary{};
        }
    }

private:
    constexpr static int XOR_MAX_GROUP_SIZE{5};
    std::unordered_map<int, std::pair<int, std::vector<Bytes>>> video_packet_sets;
    std::unordered_map<int, TimePoint> video_frame_send_times;
    HandleReceiverMessageSummary summary;
};

class AudioPacketSender
{
public:
    AudioPacketSender()
        : audio_{}, kinect_microphone_stream_{create_kinect_microphone_stream(audio_)},
        audio_encoder_{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false}, pcm_{}, audio_frame_id_{0}
    {
        constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio_.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::exception("Failed in soundio_ring_buffer_create()...");

        kinect_microphone_stream_.start();
    }

    void send(int session_id, UdpSocket& udp_socket)
    {
        soundio_flush_events(audio_.get());
        char* read_ptr{soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer)};
        int fill_bytes{soundio_ring_buffer_fill_count(soundio_callback::ring_buffer)};

        const int BYTES_PER_FRAME{gsl::narrow_cast<int>(sizeof(float) * pcm_.size())};
        int cursor = 0;
        while ((fill_bytes - cursor) > BYTES_PER_FRAME) {
            memcpy(pcm_.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder_.encode(opus_frame.data(), pcm_.data(), KH_SAMPLES_PER_FRAME, opus_frame.size());
            opus_frame.resize(opus_frame_size);

            std::error_code error;
            udp_socket.send(create_audio_sender_packet_bytes(session_id, audio_frame_id_++, opus_frame));

            cursor += BYTES_PER_FRAME;
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);
    }

private:
    Audio audio_;
    AudioInStream kinect_microphone_stream_;
    AudioEncoder audio_encoder_;

    std::array<float, KH_SAMPLES_PER_FRAME* KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};

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

void read_video_frames(const int session_id,
                       bool& stopped,
                       UdpSocket& udp_socket,
                       KinectDevice& kinect_device,
                       moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
                       ReceiverState& receiver_state,
                       TimePoint sender_start_time)
{
    constexpr short CHANGE_THRESHOLD{10};
    constexpr int INVALID_THRESHOLD{2};

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
    VideoReaderState video_state;

    // Variables for profiling the sender.
    //auto last_device_time_stamp{std::chrono::microseconds::zero()};

    ReadVideoFrameSummary summary;
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

        constexpr float AZURE_KINECT_FRAME_RATE = 30.0f;
        const auto frame_time_point{TimePoint::now()};
        const auto frame_time_diff{frame_time_point - video_state.last_frame_time_point};
        const int frame_id_diff{video_state.frame_id - receiver_state.video_frame_id};
        
        if ((frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) < std::pow(2, frame_id_diff - 3))
            continue;

        video_state.last_frame_time_point = frame_time_point;

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

        const float frame_time_stamp{(frame_time_point - sender_start_time).ms()};
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

void start(int port, int session_id, KinectDevice& kinect_device)
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
    TimePoint sender_start_time{TimePoint::now()};
    std::thread task_thread([&] {
        try {
            VideoPacketSender video_packet_sender;
            AudioPacketSender audio_packet_sender;
            while (!stopped) {
                video_packet_sender.send(session_id, udp_socket, video_packet_queue, receiver_state);
                audio_packet_sender.send(session_id, udp_socket);
            }
        } catch (UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError from task_thread:\n  " << e.what() << "\n";
        }
        stopped = true;
    });

    try {
        read_video_frames(session_id, stopped, udp_socket, kinect_device, video_packet_queue, receiver_state, sender_start_time);
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
