#include <filesystem>
#include "kh_core.h"
#include "kh_sender.h"
#include "kh_depth_compression_helper.h"
#include "azure_kinect/azure_kinect.h"
#include "k4arecord/playback.h"
#include "turbojpeg.h"
#include "k4arecord/playback.hpp"

namespace kh
{
std::vector<std::string> get_filenames_from_folder_path(std::string folder_path)
{
    std::vector<std::string> filenames;
    for (const auto& entry : std::filesystem::directory_iterator(folder_path)) {
        std::string filename = entry.path().filename().string();
        if (filename == ".gitignore")
            continue;
        if (entry.is_directory())
            continue;
        filenames.push_back(filename);
    }

    return filenames;
}

// Sends Azure Kinect frames through a TCP port.
void play_azure_kinect_frames(std::string path, int port, DepthCompressionType type)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;

    std::cout << "DepthCompressionType: " << static_cast<int>(type) << std::endl;

    std::cout << "Start sending Azure Kinect frames (port: " << port << ")." << std::endl;

    //auto playback = azure_kinect::obtainAzureKinectPlayback(path);
    //if (!playback) {
    //    std::cout << "Could not find the playback." << std::endl;
    //    return;
    //}
    auto playback = k4a::playback::open(path.c_str());

    //auto calibration = playback->getCalibration();
    //if (!calibration) {
    //    std::cout << "Could not find calibration information from playback." << std::endl;
    //    return;
    //}
    auto calibration = playback.get_calibration();

    Vp8Encoder vp8_encoder(calibration.color_camera_calibration.resolution_width,
                           calibration.color_camera_calibration.resolution_height,
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

    // The amount of frames this sender will send before receiveing a feedback from a receiver.
    const int MAXIMUM_FRAME_ID_DIFF = 2;
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
        if (!playback.get_next_capture(&capture)) {
            std::cout << "get_capture() timed out" << std::endl;
            continue;
        }

        auto jpeg_color_image = capture.get_color_image();
        if (!jpeg_color_image)
            continue;

        auto depth_image = capture.get_depth_image();
        if (!depth_image)
            continue;

        auto format = jpeg_color_image.get_format();
        if (format != K4A_IMAGE_FORMAT_COLOR_MJPG) {
            std::cout << "Color format not supported. Please use MJPEG." << std::endl;
            return;
        }

        //k4a_image_t bgra_color_image_handle;
        //if (K4A_RESULT_SUCCEEDED != k4a_image_create(K4A_IMAGE_FORMAT_COLOR_BGRA32,
        //                                             jpeg_color_image.get_width_pixels(),
        //                                             jpeg_color_image.get_height_pixels(),
        //                                             jpeg_color_image.get_width_pixels() * 4 * (int)sizeof(uint8_t),
        //                                             &bgra_color_image_handle)) {
        //    std::cout << "Failed to create image buffer" << std::endl;
        //    return;
        //}

        //auto bgra_color_image = azure_kinect::AzureKinectImage(bgra_color_image_handle);
        auto bgra_color_image = k4a::image::create(K4A_IMAGE_FORMAT_COLOR_BGRA32,
                                                   jpeg_color_image.get_width_pixels(),
                                                   jpeg_color_image.get_height_pixels(),
                                                   jpeg_color_image.get_width_pixels() * 4 * (int)sizeof(uint8_t));

        tjhandle tjHandle;
        tjHandle = tjInitDecompress();
        if (tjDecompress2(tjHandle,
                          jpeg_color_image.get_buffer(),
                          static_cast<unsigned long>(jpeg_color_image.get_size()),
                          bgra_color_image.get_buffer(),
                          jpeg_color_image.get_width_pixels(),
                          0, // pitch
                          jpeg_color_image.get_height_pixels(),
                          TJPF_BGRA,
                          TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE) != 0)
        {
            std::cout << "Failed to decompress color frame" << std::endl;
            if (tjDestroy(tjHandle))
                std::cout << "Failed to destroy turboJPEG handle." << std::endl;

            return;
        }
        if (tjDestroy(tjHandle))
            std::cout << "Failed to destroy turboJPEG handle." << std::endl;

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectYuy2Buffer(bgra_color_image.get_buffer(),
                                                                 bgra_color_image.get_width_pixels(),
                                                                 bgra_color_image.get_height_pixels(),
                                                                 bgra_color_image.get_stride_bytes());
        auto vp8_frame = vp8_encoder.encode(yuv_image);

        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder->encode(reinterpret_cast<short*>(depth_image.get_buffer()));

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            auto end = std::chrono::system_clock::now();
            std::chrono::duration<double> diff = end - start;
            std::cout << "Sending frame " << frame_id << ", "
                      << "FPS: " << frame_count / diff.count() << ", "
                      << "Bandwidth: " << frame_size / (diff.count() * 131072) << " Mbps.\r"; // 131072 = 1024 * 1024 / 8
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
    // TODO: Fix this to make it work both in Visual Studio and as an exe file.
    const std::string PLAYBACK_FOLDER_PATH = "../../../../playback/";

    for (;;) {
        std::vector<std::string> filenames(get_filenames_from_folder_path(PLAYBACK_FOLDER_PATH));

        std::cout << "Input filenames inside the playback folder:" << std::endl;
        for (int i = 0; i < filenames.size(); ++i) {
            std::cout << "\t(" << i << ") " << filenames[i] << std::endl;
        }

        int filename_index;
        for (;;) {
            std::cout << "Enter filename index: ";
            std::cin >> filename_index;
            if (filename_index >= 0 && filename_index < filenames.size())
                break;

            std::cout << "Invliad index." << std::endl;
        }

        std::string filename = filenames[filename_index];
        std::string file_path = PLAYBACK_FOLDER_PATH + filename;

        std::cout << "Enter a port number to start sending frames: ";
        std::string line;
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        int port = line.empty() ? 7777 : std::stoi(line);

        std::cout << "Enter depth compression type (1: RVL, 2: TRVL, 3: VP8): ";
        std::getline(std::cin, line);

        // The default type is TRVL.
        DepthCompressionType type = DepthCompressionType::Trvl;
        if (line == "1") {
            type = DepthCompressionType::Rvl;
        } else if (line == "2") {
            type = DepthCompressionType::Trvl;
        } else if (line == "3") {
            type = DepthCompressionType::Vp8;
        }

        try {
            play_azure_kinect_frames(file_path, port, type);
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