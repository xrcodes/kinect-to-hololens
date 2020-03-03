#include <iostream>
#include <gsl/gsl>
#include "helper/opencv_helper.h"
#include "k4a/k4a.hpp"
#include "kh_vp8.h"
#include "kh_trvl.h"

extern "C"
{
#include <libavcodec/avcodec.h>
}

namespace kh
{
void display_frames()
{
    constexpr int TARGET_BITRATE{2000};
    constexpr short CHANGE_THRESHOLD{10};
    constexpr int INVALID_THRESHOLD{2};
    constexpr auto TIMEOUT{std::chrono::milliseconds{1000}};

    // Obtain device to access Kinect frames.
    auto device{k4a::device::open(K4A_DEVICE_DEFAULT)};

    auto configuration{K4A_DEVICE_CONFIG_INIT_DISABLE_ALL};
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;
    
    const auto calibration{device.get_calibration(configuration.depth_mode, configuration.color_resolution)};
    const k4a::transformation transformation{calibration};

    Vp8Encoder vp8_encoder{calibration.depth_camera_calibration.resolution_width,
                           calibration.depth_camera_calibration.resolution_height,
                           TARGET_BITRATE};
    Vp8Decoder vp8_decoder;

    const int depth_frame_width{calibration.depth_camera_calibration.resolution_width};
    const int depth_frame_height{calibration.depth_camera_calibration.resolution_height};
    const int depth_frame_size{depth_frame_width * depth_frame_height};

    TrvlEncoder depth_encoder{depth_frame_size, CHANGE_THRESHOLD, INVALID_THRESHOLD};
    TrvlDecoder depth_decoder{depth_frame_size};

    device.start_cameras(&configuration);

    for (;;) {
        k4a::capture capture;
        if (!device.get_capture(&capture, TIMEOUT)) {
            printf("get_capture() timed out...\n");
            continue;
        }

        const auto color_image{capture.get_color_image()};
        if (!color_image) {
            printf("get_color_image() failed...\n");
            continue;
        }

        const auto depth_image{capture.get_depth_image()};
        if (!depth_image) {
            printf("get_depth_image() failed...\n");
            continue;
        }

        const auto transformed_color_image{transformation.color_image_to_depth_camera(depth_image, color_image)};

        // Encodes and decodes color pixels just to test whether Vp8Encoder and Vp8Decoder works.
        // Then, converts the pixels for OpenCV.
        const auto yuv_image{createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                     transformed_color_image.get_width_pixels(),
                                                                     transformed_color_image.get_height_pixels(),
                                                                     transformed_color_image.get_stride_bytes())};

        const auto vp8_frame{vp8_encoder.encode(yuv_image, false)};
        const auto ffmpeg_frame{vp8_decoder.decode(vp8_frame)};
        const auto color_mat{createCvMatFromYuvImage(createYuvImageFromAvFrame(*ffmpeg_frame.av_frame()))};


        // Compresses and decompresses the depth pixels to test the compression and decompression functions.
        // Then, converts the pixels for OpenCV.
        const auto depth_encoder_frame{depth_encoder.encode({reinterpret_cast<const short*>(depth_image.get_buffer()),
                                                             gsl::narrow_cast<ptrdiff_t>(depth_image.get_size())},
                                                            false)};
        auto depth_pixels{depth_decoder.decode(depth_encoder_frame, false)};
        auto depth_mat{createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_pixels.data()), 
                                                       depth_image.get_width_pixels(),
                                                       depth_image.get_height_pixels())};

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void display_calibration()
{
    auto device{k4a::device::open(K4A_DEVICE_DEFAULT)};

    k4a_device_configuration_t configuration{K4A_DEVICE_CONFIG_INIT_DISABLE_ALL};
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_YUY2;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;

    const auto calibration{device.get_calibration(configuration.depth_mode, configuration.color_resolution)};

    const auto color_intrinsics{calibration.color_camera_calibration.intrinsics};
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

    const auto depth_intrinsics{calibration.depth_camera_calibration.intrinsics};
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

    const auto extrinsics{calibration.extrinsics[K4A_CALIBRATION_TYPE_COLOR][K4A_CALIBRATION_TYPE_DEPTH]};
    for (gsl::index i = 0; i < 9; ++i)
        std::cout << "extrinsic rotation[" << i << "]: " << extrinsics.rotation[i] << std::endl;

    for (gsl::index i = 0; i < 3; ++i)
        std::cout << "extrinsic translation[" << i << "]: " << extrinsics.translation[i] << std::endl;
}

void main()
{
    for (;;) {
        std::cout << "Press Enter to Start: ";
        std::string line;
        std::getline(std::cin, line);

        // If "calibration" is entered, prints calibration information instead of displaying frames.
        if (line == "calibration") {
            display_calibration();
        } else {
            display_frames();
        }
    }
}
}

int main()
{
    kh::main();
    return 0;
}