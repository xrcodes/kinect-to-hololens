#include <iostream>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kinect/kinect.h"
#include "azure_kinect/azure_kinect.h"

namespace kh
{
void _display_azure_kinect_frames()
{
    const int TARGET_BITRATE = 2000;
    const int32_t TIMEOUT_IN_MS = 1000;

    // Obtain device to access Kinect frames.
    auto device = azure_kinect::obtainAzureKinectDevice();
    if (!device) {
        std::cout << "Could not find an Azure Kinect." << std::endl;
        return;
    }

    k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.color_format = K4A_IMAGE_FORMAT_COLOR_YUY2;
    config.color_resolution = K4A_COLOR_RESOLUTION_720P;
    config.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    config.camera_fps = K4A_FRAMES_PER_SECOND_30;

    auto calibration = device->getCalibration(config.depth_mode, config.color_resolution);
    if (!calibration) {
        std::cout << "Failed to receive calibration of the Azure Kinect." << std::endl;
        return;
    }

    Vp8Encoder encoder(calibration->color_camera_calibration.resolution_width,
                       calibration->color_camera_calibration.resolution_height,
                       TARGET_BITRATE);
    Vp8Decoder decoder;

    if (!device->start(config)) {
        std::cout << "Failed to start the Azure Kinect." << std::endl;
        return;
    }

    for (;;) {
        // Try getting Kinect images until a valid pair pops up.
        auto capture = device->getCapture(TIMEOUT_IN_MS);
        if (!capture) {
            std::cout << "Could not get a capture from the Azure Kinect." << std::endl;
            return;
        }

        auto color_image = capture->getColorImage();
        if (!color_image)
            continue;

        auto depth_image = capture->getDepthImage();
        if (!depth_image)
            continue;

        // Encodes and decodes color pixels just to test whether Vp8Encoder and Vp8Decoder works.
        // Then, converts the pixels for OpenCV.
        auto yuv_image = createYuvImageFromAzureKinectYuy2Buffer(color_image->getBuffer(),
                                                                 color_image->getWidth(),
                                                                 color_image->getHeight(),
                                                                 color_image->getStride());
        auto vp8_frame = encoder.encode(yuv_image);
        auto ffmpeg_frame = decoder.decode(vp8_frame.data(), vp8_frame.size());
        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame.av_frame()));

        // Compresses and decompresses the depth pixels to test the compression and decompression functions.
        // Then, converts the pixels for OpenCV.
        auto rvl_frame = createRvlFrameFromKinectDepthBuffer(reinterpret_cast<uint16_t*>(depth_image->getBuffer()),
                                                             depth_image->getWidth(),
                                                             depth_image->getHeight());
        auto depth_pixels = createDepthImageFromRvlFrame(rvl_frame.data(), depth_image->getWidth(), depth_image->getHeight());
        auto depth_mat = createCvMatFromKinectDepthImage(depth_pixels.data(), depth_image->getWidth(), depth_image->getHeight());

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void _display_azure_kinect_intrinsics()
{
    auto device = azure_kinect::obtainAzureKinectDevice();
    if (!device) {
        std::cout << "Could not find an Azure Kinect." << std::endl;
    }

    auto calibration = device->getCalibration(K4A_DEPTH_MODE_NFOV_UNBINNED, K4A_COLOR_RESOLUTION_720P);
    if (!calibration) {
        std::cout << "Failed to get calibration information of the Azure Kinect." << std::endl;
    }

    auto intrinsics = calibration->color_camera_calibration.intrinsics;
    std::cout << "intrinsics type: " << intrinsics.type << std::endl;
    std::cout << "intrinsics parameter_count: " << intrinsics.parameter_count << std::endl;
    std::cout << "intrinsics cx: " << intrinsics.parameters.param.cx << std::endl;
    std::cout << "intrinsics cy: " << intrinsics.parameters.param.cy << std::endl;
    std::cout << "intrinsics fx: " << intrinsics.parameters.param.fx << std::endl;
    std::cout << "intrinsics fy: " << intrinsics.parameters.param.fy << std::endl;
    std::cout << "intrinsics k1: " << intrinsics.parameters.param.k1 << std::endl;
    std::cout << "intrinsics k2: " << intrinsics.parameters.param.k2 << std::endl;
    std::cout << "intrinsics k3: " << intrinsics.parameters.param.k3 << std::endl;
    std::cout << "intrinsics k4: " << intrinsics.parameters.param.k4 << std::endl;
    std::cout << "intrinsics k5: " << intrinsics.parameters.param.k5 << std::endl;
    std::cout << "intrinsics k6: " << intrinsics.parameters.param.k6 << std::endl;
    std::cout << "intrinsics cody: " << intrinsics.parameters.param.codx << std::endl;
    std::cout << "intrinsics codx: " << intrinsics.parameters.param.cody << std::endl;
    std::cout << "intrinsics p2: " << intrinsics.parameters.param.p2 << std::endl;
    std::cout << "intrinsics p1: " << intrinsics.parameters.param.p1 << std::endl;
    std::cout << "intrinsics metric_radius: " << intrinsics.parameters.param.metric_radius << std::endl;
}

void display_frames()
{
    for (;;) {
        std::cout << "Press enter to display frames." << std::endl;
        std::string line;
        std::getline(std::cin, line);
        // If "intrinsics" is entered, prints KinectIntrinsics instead of displaying frames.
        // A kind of an easter egg.
        // Usually, Kinect frames are displayed.
        if (line == "intrinsics") {
            //_display_intrinsics();
            _display_azure_kinect_intrinsics();
        } else {
            //_display_frames();
            _display_azure_kinect_frames();
        }
    }
}
}

int main()
{
    kh::display_frames();
    return 0;
}