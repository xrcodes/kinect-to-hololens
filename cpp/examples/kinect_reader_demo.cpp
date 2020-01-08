#include <iostream>
#include "helper/opencv_helper.h"
#include "kh_depth_compression_helper.h"
#include "k4a/k4a.hpp"

namespace kh
{
void display_frames(DepthCompressionType type)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const auto TIMEOUT = std::chrono::milliseconds(1000);

    // Obtain device to access Kinect frames.
    auto device = k4a::device::open(K4A_DEVICE_DEFAULT);

    auto configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;
    
    auto calibration = device.get_calibration(configuration.depth_mode, configuration.color_resolution);
    k4a::transformation transformation(calibration);

    Vp8Encoder vp8_encoder(calibration.depth_camera_calibration.resolution_width,
                           calibration.depth_camera_calibration.resolution_height,
                           TARGET_BITRATE);
    Vp8Decoder vp8_decoder;

    int depth_frame_width = calibration.depth_camera_calibration.resolution_width;
    int depth_frame_height = calibration.depth_camera_calibration.resolution_height;
    int depth_frame_size = depth_frame_width * depth_frame_height;

    std::unique_ptr<DepthEncoder> depth_encoder;
    std::unique_ptr<DepthDecoder> depth_decoder;

    if (type == DepthCompressionType::Rvl) {
        depth_encoder = std::make_unique<RvlDepthEncoder>(depth_frame_size);
        depth_decoder = std::make_unique<RvlDepthDecoder>(depth_frame_size);
    } else if (type == DepthCompressionType::Trvl) {
        depth_encoder = std::make_unique<TrvlDepthEncoder>(depth_frame_size, CHANGE_THRESHOLD, INVALID_THRESHOLD);
        depth_decoder = std::make_unique<TrvlDepthDecoder>(depth_frame_size);
    } else if (type == DepthCompressionType::Vp8) {
        depth_encoder = std::make_unique<Vp8DepthEncoder>(depth_frame_width, depth_frame_height, TARGET_BITRATE);
        depth_decoder = std::make_unique<Vp8DepthDecoder>();
    }

    device.start_cameras(&configuration);

    for (;;) {
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

        // Encodes and decodes color pixels just to test whether Vp8Encoder and Vp8Decoder works.
        // Then, converts the pixels for OpenCV.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());

        auto vp8_frame = vp8_encoder.encode(yuv_image);
        auto ffmpeg_frame = vp8_decoder.decode(vp8_frame.data(), vp8_frame.size());
        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame.av_frame()));

        // Compresses and decompresses the depth pixels to test the compression and decompression functions.
        // Then, converts the pixels for OpenCV.
        auto depth_encoder_frame = depth_encoder->encode(reinterpret_cast<short*>(depth_image.get_buffer()));
        auto depth_pixels = depth_decoder->decode(depth_encoder_frame.data(), depth_encoder_frame.size());
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_pixels.data()), depth_image.get_width_pixels(), depth_image.get_height_pixels());

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void _display_calibration()
{
    auto device = k4a::device::open(K4A_DEVICE_DEFAULT);

    k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_YUY2;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;

    auto calibration = device.get_calibration(configuration.depth_mode, configuration.color_resolution);

    auto color_intrinsics = calibration.color_camera_calibration.intrinsics;
    std::cout << "color camera metric_radius: " << calibration.color_camera_calibration.metric_radius << std::endl;

    std::cout << "color intrinsics type: " << color_intrinsics.type << std::endl;
    std::cout << "color intrinsics parameter_count: " << color_intrinsics.parameter_count << std::endl;

    std::cout << "color intrinsics cx: " << color_intrinsics.parameters.param.cx << std::endl;
    std::cout << "color intrinsics cy: " << color_intrinsics.parameters.param.cy << std::endl;
    std::cout << "color intrinsics fx: " << color_intrinsics.parameters.param.fx << std::endl;
    std::cout << "color intrinsics fy: " << color_intrinsics.parameters.param.fy << std::endl;
    std::cout << "color intrinsics k1: " << color_intrinsics.parameters.param.k1 << std::endl;
    std::cout << "color intrinsics k2: " << color_intrinsics.parameters.param.k2 << std::endl;
    std::cout << "color intrinsics k3: " << color_intrinsics.parameters.param.k3 << std::endl;
    std::cout << "color intrinsics k4: " << color_intrinsics.parameters.param.k4 << std::endl;
    std::cout << "color intrinsics k5: " << color_intrinsics.parameters.param.k5 << std::endl;
    std::cout << "color intrinsics k6: " << color_intrinsics.parameters.param.k6 << std::endl;
    std::cout << "color intrinsics cody: " << color_intrinsics.parameters.param.codx << std::endl;
    std::cout << "color intrinsics codx: " << color_intrinsics.parameters.param.cody << std::endl;
    std::cout << "color intrinsics p2: " << color_intrinsics.parameters.param.p2 << std::endl;
    std::cout << "color intrinsics p1: " << color_intrinsics.parameters.param.p1 << std::endl;
    std::cout << "color intrinsics metric_radius: " << color_intrinsics.parameters.param.metric_radius << std::endl;

    auto depth_intrinsics = calibration.depth_camera_calibration.intrinsics;
    std::cout << "depth camera metric_radius: " << calibration.depth_camera_calibration.metric_radius << std::endl;

    std::cout << "depth intrinsics type: " << depth_intrinsics.type << std::endl;
    std::cout << "depth intrinsics parameter_count: " << depth_intrinsics.parameter_count << std::endl;

    std::cout << "depth intrinsics cx: " << depth_intrinsics.parameters.param.cx << std::endl;
    std::cout << "depth intrinsics cy: " << depth_intrinsics.parameters.param.cy << std::endl;
    std::cout << "depth intrinsics fx: " << depth_intrinsics.parameters.param.fx << std::endl;
    std::cout << "depth intrinsics fy: " << depth_intrinsics.parameters.param.fy << std::endl;
    std::cout << "depth intrinsics k1: " << depth_intrinsics.parameters.param.k1 << std::endl;
    std::cout << "depth intrinsics k2: " << depth_intrinsics.parameters.param.k2 << std::endl;
    std::cout << "depth intrinsics k3: " << depth_intrinsics.parameters.param.k3 << std::endl;
    std::cout << "depth intrinsics k4: " << depth_intrinsics.parameters.param.k4 << std::endl;
    std::cout << "depth intrinsics k5: " << depth_intrinsics.parameters.param.k5 << std::endl;
    std::cout << "depth intrinsics k6: " << depth_intrinsics.parameters.param.k6 << std::endl;
    std::cout << "depth intrinsics cody: " << depth_intrinsics.parameters.param.codx << std::endl;
    std::cout << "depth intrinsics codx: " << depth_intrinsics.parameters.param.cody << std::endl;
    std::cout << "depth intrinsics p2: " << depth_intrinsics.parameters.param.p2 << std::endl;
    std::cout << "depth intrinsics p1: " << depth_intrinsics.parameters.param.p1 << std::endl;
    std::cout << "depth intrinsics metric_radius: " << depth_intrinsics.parameters.param.metric_radius << std::endl;

    auto extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];
    for (int i = 0; i < 9; ++i)
        std::cout << "extrinsic rotation[" << i << "]: " << extrinsics.rotation[i] << std::endl;

    for (int i = 0; i < 3; ++i)
        std::cout << "extrinsic translation[" << i << "]: " << extrinsics.translation[i] << std::endl;
}

void display_frames()
{
    for (;;) {
        std::cout << "Enter depth compression type (RVL: 1, TRVL: 2, VP8: 3): ";
        std::string line;
        std::getline(std::cin, line);

        // If "calibration" is entered, prints calibration information instead of displaying frames.
        if (line == "calibration") {
            _display_calibration();
            continue;
        }

        if (line == "1") {
            display_frames(DepthCompressionType::Rvl);
        } else if (line == "2") {
            display_frames(DepthCompressionType::Trvl);
        } else if (line == "3") {
            display_frames(DepthCompressionType::Vp8);
        } else {
            // Use TRVL as the default type.
            display_frames(DepthCompressionType::Trvl);
        }

    }
}
}

int main()
{
    kh::display_frames();
    return 0;
}