#include <iostream>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_rvl.h"
#include "kh_trvl.h"
#include "azure_kinect/azure_kinect.h"

namespace kh
{
void _display_azure_kinect_frames()
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const int32_t TIMEOUT_IN_MS = 1000;

    // Obtain device to access Kinect frames.
    auto device = azure_kinect::obtainAzureKinectDevice();
    if (!device) {
        std::cout << "Could not find an Azure Kinect." << std::endl;
        return;
    }
    
    auto configuration = azure_kinect::getDefaultDeviceConfiguration();
    auto calibration = device->getCalibration(configuration.depth_mode, configuration.color_resolution);
    if (!calibration) {
        std::cout << "Failed to receive calibration of the Azure Kinect." << std::endl;
        return;
    }

    Vp8Encoder vp8_encoder(calibration->color_camera_calibration.resolution_width,
                           calibration->color_camera_calibration.resolution_height,
                           TARGET_BITRATE);
    Vp8Decoder vp8_decoder;

    int depth_frame_size = calibration->depth_camera_calibration.resolution_width
                         * calibration->depth_camera_calibration.resolution_height;
    TrvlEncoder trvl_encoder(depth_frame_size, CHANGE_THRESHOLD, INVALID_THRESHOLD);
    TrvlDecoder trvl_decoder(depth_frame_size);

    if (!device->start(configuration)) {
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
        auto vp8_frame = vp8_encoder.encode(yuv_image);
        auto ffmpeg_frame = vp8_decoder.decode(vp8_frame.data(), vp8_frame.size());
        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame.av_frame()));

        // Compresses and decompresses the depth pixels to test the compression and decompression functions.
        // Then, converts the pixels for OpenCV.
        //auto rvl_frame = rvl::compress(reinterpret_cast<short*>(depth_image->getBuffer()), depth_image->getWidth() * depth_image->getHeight());
        //auto depth_pixels = rvl::decompress(rvl_frame.data(), depth_image->getWidth() * depth_image->getHeight());

        auto trvl_frame = trvl_encoder.encode(reinterpret_cast<short*>(depth_image->getBuffer()));
        auto depth_pixels = trvl_decoder.decode(trvl_frame.data());
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_pixels.data()), depth_image->getWidth(), depth_image->getHeight());

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void _display_azure_kinect_calibration()
{
    auto device = azure_kinect::obtainAzureKinectDevice();
    if (!device) {
        std::cout << "Could not find an Azure Kinect." << std::endl;
    }


    auto configuration = azure_kinect::getDefaultDeviceConfiguration();
    auto calibration = device->getCalibration(configuration.depth_mode, configuration.color_resolution);
    if (!calibration) {
        std::cout << "Failed to get calibration information of the Azure Kinect." << std::endl;
    }

    auto color_intrinsics = calibration->color_camera_calibration.intrinsics;
    std::cout << "color camera metric_radius: " << calibration->color_camera_calibration.metric_radius << std::endl;

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

    auto depth_intrinsics = calibration->depth_camera_calibration.intrinsics;
    std::cout << "depth camera metric_radius: " << calibration->depth_camera_calibration.metric_radius << std::endl;

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

    auto extrinsics = calibration->extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];
    for (int i = 0; i < 9; ++i)
        std::cout << "extrinsic rotation[" << i << "]: " << extrinsics.rotation[i] << std::endl;

    for (int i = 0; i < 3; ++i)
        std::cout << "extrinsic translation[" << i << "]: " << extrinsics.translation[i] << std::endl;
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
        if (line == "calibration") {
            _display_azure_kinect_calibration();
            continue;
        }

        _display_azure_kinect_frames();
    }
}
}

int main()
{
    kh::display_frames();
    return 0;
}