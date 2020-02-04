#include <chrono>
#include <iostream>
#include <random>
#include "helper/kinect_helper.h"
#include "kh_core.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "kh_sender.h"

namespace kh
{

// Sends Azure Kinect frames through a TCP port.
void _send_frames(int session_id, KinectDevice& device, int port)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;

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

    std::array<char, 1> recv_buf;
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(recv_buf), remote_endpoint, 0, error);

    if (error) {
        std::cout << "Error receiving remote_endpoint: " << error.message() << std::endl;
        return;
    }

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    Sender sender(std::move(socket), remote_endpoint, 1024 * 1024);
    sender.send(session_id, calibration);

    // frame_id is the ID of the frame the sender sends.
    int frame_id = 0;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id = 0;

    // Variables for profiling the sender.
    int frame_count = 0;
    int keyframe_count = 0;
    std::chrono::microseconds last_time_stamp;

    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> frame_start_times;
    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> frame_start_times2;
    auto summary_start = std::chrono::steady_clock::now();
    size_t frame_size = 0;
    for (;;) {
        auto receive_result = sender.receive();
        if (receive_result) {
            uint8_t message_type = (*receive_result)[0];
            float packet_collection_ms;
            float decoder_ms;
            float frame_ms;
            memcpy(&receiver_frame_id, receive_result->data() + 1, 4);
            memcpy(&packet_collection_ms, receive_result->data() + 5, 4);
            memcpy(&decoder_ms, receive_result->data() + 9, 4);
            memcpy(&frame_ms, receive_result->data() + 13, 4);

            auto return_time = std::chrono::steady_clock::now();
            std::chrono::duration<double> round_trip_time = return_time - frame_start_times[receiver_frame_id];
            std::chrono::duration<double> round_trip_time2 = return_time - frame_start_times2[receiver_frame_id];

            printf("id: %d, packet: %f, decoder: %f, frame: %f, round_trip: %f, round_trip2: %f\n",
                   receiver_frame_id, packet_collection_ms, decoder_ms, frame_ms,
                   round_trip_time.count() * 1000.0f, round_trip_time2.count() * 1000.0f);

            //std::cout << "now: " << std::chrono::steady_clock::now().time_since_epoch().count() << std::endl;
            //std::cout << "receiver_frame_id: " << receiver_frame_id << std::endl;
            std::vector<int> obsolete_frame_ids;
            for (auto frame_start_time_pair : frame_start_times) {
                //std::cout << "frame_start_times key: " << frame_start_time_pair.first << std::endl;
                //std::cout << "frame_start_times value: " << frame_start_time_pair.second.time_since_epoch().count() << std::endl;
                if (frame_start_time_pair.first <= receiver_frame_id) {
                    obsolete_frame_ids.push_back(frame_start_time_pair.first);
                }
            }

            for (int obsolete_frame_id : obsolete_frame_ids) {
                frame_start_times.erase(obsolete_frame_id);
            }

            std::vector<int> obsolete_frame_ids2;
            for (auto frame_start_time_pair2 : frame_start_times2) {
                if (frame_start_time_pair2.first <= receiver_frame_id) {
                    obsolete_frame_ids2.push_back(frame_start_time_pair2.first);
                }
            }

            for (int obsolete_frame_id2 : obsolete_frame_ids2) {
                frame_start_times2.erase(obsolete_frame_id2);
            }
        }

        frame_start_times[frame_id] = std::chrono::steady_clock::now();

        auto capture = device.getCapture();
        if (!capture)
            continue;

        auto color_image = capture->get_color_image();
        if (!color_image) {
            std::cout << "no color_image" << std::endl;
            continue;
        }

        int frame_id_diff = frame_id - receiver_frame_id;

        auto time_stamp = color_image.get_device_timestamp();
        auto time_diff = time_stamp - last_time_stamp;
        // Rounding assuming that the framerate is 30 Hz.
        int device_frame_diff = (int)(time_diff.count() / 33000.0f + 0.5f);

        //if (frame_id != 0 && device_frame_diff < pow_of_two(frame_id_diff - 1) / 4) {
        //    continue;
        //}

        auto depth_image = capture->get_depth_image();
        if (!depth_image) {
            std::cout << "no depth_image" << std::endl;
            continue;
        }

        float frame_time_stamp = time_stamp.count() / 1000.0f;

        auto transformed_color_image = transformation.color_image_to_depth_camera(depth_image, color_image);

        bool keyframe = frame_id_diff > 4;

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());
        auto vp8_frame = color_encoder.encode(yuv_image, keyframe);

        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder.encode(reinterpret_cast<short*>(depth_image.get_buffer()), keyframe);

        frame_start_times2[frame_id] = std::chrono::steady_clock::now();

        sender.send(session_id, frame_id, frame_time_stamp, keyframe, vp8_frame,
                    reinterpret_cast<uint8_t*>(depth_encoder_frame.data()), depth_encoder_frame.size());

        last_time_stamp = time_stamp;

        // Updating variables for profiling.
        ++frame_count;
        if (keyframe)
            ++keyframe_count;

        frame_size += vp8_frame.size() + depth_encoder_frame.size();

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            auto summary_end = std::chrono::steady_clock::now();
            std::chrono::duration<double> diff = summary_end - summary_start;
            std::stringstream ss;
            ss << "Summary for frame " << frame_id << ", "
                << "FPS: " << frame_count / diff.count() << ", "
                << "Keyframe Ratio: " << keyframe_count / (float) frame_count * 100.0f << "%, "
                << "Bandwidth: " << frame_size / (diff.count() * 131072) << " Mbps. "; // 131072 = 1024 * 1024 / 8
            std::cout << ss.str() << std::endl;
            summary_start = summary_end;
            frame_count = 0;
            keyframe_count = 0;
            frame_size = 0;
        }

        ++frame_id;
    }

    std::cout << "Stopped sending Kinect frames." << std::endl;
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void send_frames()
{
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    for (;;) {
        std::string line;
        std::cout << "Enter a port number to start sending frames: ";
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
            std::cout << e.what() << std::endl;
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