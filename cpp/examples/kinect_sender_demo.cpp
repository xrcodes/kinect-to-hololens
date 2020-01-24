#include "kh_core.h"
#include "kh_sender.h"
#include "kh_depth_compression_helper.h"
#include "k4a/k4a.hpp"

namespace kh
{
// Sends Azure Kinect frames through a TCP port.
void _send_azure_kinect_frames(int port, DepthCompressionType type, bool binned_depth)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const auto TIMEOUT = std::chrono::milliseconds(1000);

    std::cout << "DepthCompressionType: " << static_cast<int>(type) << std::endl;
    std::cout << "binned_depth: " << binned_depth << std::endl;

    std::cout << "Start sending Azure Kinect frames (port: " << port << ")." << std::endl;

    auto device = k4a::device::open(K4A_DEVICE_DEFAULT);

    k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;

    if(binned_depth)
        configuration.depth_mode = K4A_DEPTH_MODE_NFOV_2X2BINNED;

    auto calibration = device.get_calibration(configuration.depth_mode, configuration.color_resolution);
    k4a::transformation transformation(calibration);

    Vp8Encoder vp8_encoder(calibration.depth_camera_calibration.resolution_width,
                           calibration.depth_camera_calibration.resolution_height,
                           TARGET_BITRATE);

    int depth_frame_width = calibration.depth_camera_calibration.resolution_width;
    int depth_frame_height = calibration.depth_camera_calibration.resolution_height;
    int depth_frame_size = depth_frame_width * depth_frame_height;
    std::unique_ptr<DepthEncoder> depth_encoder;
    if (type == DepthCompressionType::Rvl) {
        depth_encoder = std::make_unique<RvlDepthEncoder>(depth_frame_size);
    } else if (type == DepthCompressionType::Trvl) {
        depth_encoder = std::make_unique<TrvlDepthEncoder>(depth_frame_size, CHANGE_THRESHOLD, INVALID_THRESHOLD);
    } else if (type == DepthCompressionType::Vp8) {
        depth_encoder = std::make_unique<Vp8DepthEncoder>(depth_frame_width, depth_frame_height, TARGET_BITRATE);
    }

    // Creating a tcp socket with the port and waiting for a connection.
    asio::io_context io_context;
    asio::ip::tcp::acceptor acceptor(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));
    auto socket = acceptor.accept();

    std::cout << "Accepted a client!" << std::endl;

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    Sender sender(std::move(socket));
    // The sender sends the KinectIntrinsics, so the renderer from the receiver side can prepare rendering Kinect frames.
    // TODO: Add a function send() for Azure Kinect.
    sender.send(static_cast<int>(type), calibration);

    device.start_cameras(&configuration);

    // The amount of frames this sender will send before receiveing a feedback from a receiver.
    const int MAXIMUM_FRAME_ID_DIFF = 3;
    // const int MAXIMUM_FRAME_ID_DIFF = 10;
    // frame_id is the ID of the frame the sender sends.
    int frame_id = 0;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id = 0;

    // Variables for profiling the sender.
    auto start = std::chrono::system_clock::now();
    int frame_count = 0;
    size_t frame_size = 0;
    for (;;) {
        // Try receiving a frame ID from the receiver and update receiver_frame_id if possible.
        auto receive_result = sender.receive();
        if (receive_result) {
            int cursor = 0;

            // Currently, message_type is not getting used.
            auto message_type = (*receive_result)[0];
            cursor += 1;

            if (message_type == 0) {
                memcpy(&receiver_frame_id, receive_result->data() + cursor, 4);
            }
        }

        // If more than MAXIMUM_FRAME_ID_DIFF frames are sent to the receiver without receiver_frame_id getting updated,
        // stop sending more.
        if (frame_id - receiver_frame_id > MAXIMUM_FRAME_ID_DIFF)
            continue;

        k4a::capture capture;
        if (!device.get_capture(&capture, TIMEOUT)) {
            std::cout << "get_capture() timed out" << std::endl;
            continue;
        }

        auto color_image = capture.get_color_image();
        if (!color_image) {
            std::cout << "no color_image" << std::endl;
            continue;
        }

        auto depth_image = capture.get_depth_image();
        if (!depth_image) {
            std::cout << "no depth_image" << std::endl;
            continue;
        }

        auto transformed_color_image = transformation.color_image_to_depth_camera(depth_image, color_image);

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());
        auto vp8_frame = vp8_encoder.encode(yuv_image);

        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder->encode(reinterpret_cast<short*>(depth_image.get_buffer()));

        // A temporary log. Should be deleted later.
        std::cout << "diff: " << (frame_id - receiver_frame_id) << std::endl;

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> diff = end - start;
            std::stringstream ss;
            ss << "Summry for frame " << frame_id << ", "
                << "FPS: " << frame_count / diff.count() << ", "
                << "Bandwidth: " << frame_size / (diff.count() * 131072) << " Mbps.     "; // 131072 = 1024 * 1024 / 8
            // White spaces are added at the end to make sure to clean up the previous line.
            std::cout << ss.str() << "       \r";
            start = end;
            frame_count = 0;
            frame_size = 0;
        }

        // Try sending the frame. Escape the loop if there is a network error.
        try {
            sender.send(frame_id++, vp8_frame, reinterpret_cast<uint8_t*>(depth_encoder_frame.data()), depth_encoder_frame.size());
        } catch (std::exception & e) {
            std::cout << e.what() << std::endl;
            break;
        }

        // Updating variables for profiling.
        ++frame_count;
        //frame_size += vp8_frame.size() + rvl_frame.size();
        frame_size += vp8_frame.size() + depth_encoder_frame.size();
    }

    std::cout << "Stopped sending Kinect frames." << std::endl;
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void send_frames()
{
    for (;;) {
        std::cout << "Enter a port number to start sending frames: ";
        std::string line;
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        int port = line.empty() ? 7777 : std::stoi(line);

        std::cout << "Enter depth compression type (1: RVL, 2: TRVL, 3: VP8, 4: Binned TRVL): ";
        std::getline(std::cin, line);

        // The default type is TRVL.
        DepthCompressionType type = DepthCompressionType::Trvl;
        bool binned_depth = false;
        if (line == "1") {
            type = DepthCompressionType::Rvl;
        } else if (line == "2") {
            type = DepthCompressionType::Trvl;
        } else if (line == "3") {
            type = DepthCompressionType::Vp8;
        } else if (line == "4") {
            type = DepthCompressionType::Trvl;
            binned_depth = true;
        }

        try {
            _send_azure_kinect_frames(port, type, binned_depth);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }
}
}

int main()
{
    kh::send_frames();
    return 0;
}