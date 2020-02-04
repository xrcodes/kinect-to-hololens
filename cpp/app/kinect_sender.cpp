#include <chrono>
#include <iostream>
#include <random>
#include "helper/kinect_helper.h"
#include "kh_sender.h"
#include "kh_trvl.h"
#include "kh_vp8.h"

namespace kh
{

// Sends Azure Kinect frames through a TCP port.
void _send_frames(int session_id, KinectDevice& device, int port)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const int SENDER_SEND_BUFFER_SIZE = 1024 * 1024;

    printf("Start Sending Frames (session_id: %d, port: %d)\n", session_id, port);

    auto calibration = device.getCalibration();
    k4a::transformation transformation(calibration);

    int depth_width = calibration.depth_camera_calibration.resolution_width;
    int depth_height = calibration.depth_camera_calibration.resolution_height;
    
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    Vp8Encoder color_encoder(depth_width, depth_height, TARGET_BITRATE);
    TrvlEncoder depth_encoder(depth_width * depth_height, CHANGE_THRESHOLD, INVALID_THRESHOLD);

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

    std::vector<uint8_t> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);

    if (error) {
        printf("Error receiving ping: %s\n", error.message().c_str());
        throw std::system_error(error);
    }

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    Sender sender(std::move(socket), remote_endpoint, SENDER_SEND_BUFFER_SIZE);
    sender.send(session_id, calibration);

    // frame_id is the ID of the frame the sender sends.
    int frame_id = 0;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id = 0;

    // Variables for profiling the sender.
    int summary_keyframe_count = 0;
    std::chrono::microseconds last_time_stamp;

    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> frame_start_times;
    auto summary_start = std::chrono::steady_clock::now();
    size_t summary_frame_size_sum = 0;
    for (;;) {
        auto receive_result = sender.receive();
        if (receive_result) {
            int cursor = 0;
            uint8_t message_type = (*receive_result)[cursor];
            cursor += 1;

            memcpy(&receiver_frame_id, receive_result->data() + cursor, 4);
            cursor += 4;

            float packet_collection_time_ms;
            memcpy(&packet_collection_time_ms, receive_result->data() + cursor, 4);
            cursor += 4;

            float decoder_time_ms;
            memcpy(&decoder_time_ms, receive_result->data() + cursor, 4);
            cursor += 4;

            float frame_time_ms;
            memcpy(&frame_time_ms, receive_result->data() + cursor, 4);

            std::chrono::duration<double> round_trip_time = std::chrono::steady_clock::now() - frame_start_times[receiver_frame_id];

            printf("Frame id: %d, packet: %f ms, decoder: %f ms, frame: %f ms, round_trip: %f ms\n",
                   receiver_frame_id, packet_collection_time_ms, decoder_time_ms, frame_time_ms,
                   round_trip_time.count() * 1000.0f);

            std::vector<int> obsolete_frame_ids;
            for (auto frame_start_time_pair : frame_start_times) {
                if (frame_start_time_pair.first <= receiver_frame_id)
                    obsolete_frame_ids.push_back(frame_start_time_pair.first);
            }

            for (int obsolete_frame_id : obsolete_frame_ids)
                frame_start_times.erase(obsolete_frame_id);
        }

        frame_start_times[frame_id] = std::chrono::steady_clock::now();

        auto capture = device.getCapture();
        if (!capture)
            continue;

        auto color_image = capture->get_color_image();
        if (!color_image) {
            printf("get_color_image() failed...\n");
            continue;
        }


        auto depth_image = capture->get_depth_image();
        if (!depth_image) {
            printf("get_depth_image() failed...\n");
            continue;
        }

        auto time_stamp = color_image.get_device_timestamp();
        float frame_time_stamp = time_stamp.count() / 1000.0f;
        bool keyframe = (frame_id - receiver_frame_id) > 4;

        auto transformed_color_image = transformation.color_image_to_depth_camera(depth_image, color_image);

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());
        auto vp8_frame = color_encoder.encode(yuv_image, keyframe);

        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder.encode(reinterpret_cast<short*>(depth_image.get_buffer()), keyframe);

        sender.send(session_id, frame_id, frame_time_stamp, keyframe, vp8_frame,
                    reinterpret_cast<uint8_t*>(depth_encoder_frame.data()),
                    static_cast<uint32_t>(depth_encoder_frame.size()));

        last_time_stamp = time_stamp;

        // Updating variables for profiling.
        if (keyframe)
            ++summary_keyframe_count;
        summary_frame_size_sum += vp8_frame.size() + depth_encoder_frame.size();

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            std::chrono::duration<double> summary_time_interval = std::chrono::steady_clock::now() - summary_start;
            printf("Summary id: %d, FPS: %lf, Keyframe Ratio: %d%%, Bandwidth: %lf Mbps\n",
                   frame_id, 100 / summary_time_interval.count(), summary_keyframe_count,
                   summary_frame_size_sum / (summary_time_interval.count() * 131072));
            summary_start = std::chrono::steady_clock::now();
            summary_keyframe_count = 0;
            summary_frame_size_sum = 0;
        }

        ++frame_id;
    }
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void send_frames()
{
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    for (;;) {
        std::string line;
        printf("Enter a port number to start sending frames: ");
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        int port = line.empty() ? 7777 : std::stoi(line);

        k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
        configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
        configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
        configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        auto timeout = std::chrono::milliseconds(1000);

        auto device = KinectDevice::create(configuration, timeout);
        if (!device) {
            printf("Failed to create a KinectDevice...\n");
            continue;
        }
        device->start();

        int session_id = rng() % (INT_MAX + 1);

        try {
            _send_frames(session_id, *device, port);
        } catch (std::exception & e) {
            printf("Error from _send_frames: %s\n", e.what());
        }
    }
}
}

int main()
{
    srand(time(nullptr));
    kh::send_frames();
    return 0;
}