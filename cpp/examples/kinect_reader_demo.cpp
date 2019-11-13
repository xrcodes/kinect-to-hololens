#include <iostream>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kinect/kinect.h"
#include "azure_kinect/azure_kinect.h"

namespace kh
{
void _display_frames()
{
    // Obtain KinectDevice to access Kinect frames.
    auto device = kinect::obtainKinectDevice();
    if (!device) {
        std::cout << "Could not find a kinect" << std::endl;
        return;
    }

    // The width and height of the Vp8Encoder are the half of Kinect frames'
    // since frames get halved before getting encoded.
    Vp8Encoder encoder(960, 540, 2000);
    Vp8Decoder decoder;

    for (;;) {
        // Try acquiring a Kinect frame until a valid frame pops up.
        auto kinect_frame = device->acquireFrame();
        if (!kinect_frame)
            continue;

        // Encodes and decodes color pixels just to test whether Vp8Encoder and Vp8Decoder works.
        // Then, converts the pixels for OpenCV.
        auto yuv_image = createHalvedYuvImageFromKinectColorBuffer(kinect_frame->color_frame()->getRawUnderlyingBuffer());
        auto vp8_frame = encoder.encode(yuv_image);
        auto ffmpeg_frame = decoder.decode(vp8_frame.data(), vp8_frame.size());
        auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame.av_frame()));

        // Compresses and decompresses the depth pixels to test the compression and decompression functions.
        // Then, converts the pixels for OpenCV.
        auto rvl_frame = createRvlFrameFromKinectDepthBuffer(kinect_frame->depth_frame()->getUnderlyingBuffer());
        auto depth_image = createDepthImageFromRvlFrame(rvl_frame.data());
        auto depth_mat = createCvMatFromKinectDepthImage(depth_image.data());

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void _display_intrinsics()
{
    // Obtain KinectIntrinsics using Freenect2.
    auto intrinsics = kinect::obtainKinectIntrinsics();
    if (!intrinsics) {
        std::cout << "Could not find intrinsics of a Kinect." << std::endl;
        return;
    }

    // Print all parameters of KinectIntrinsics.
    std::cout << "Color fx: " << intrinsics->color_params.fx << std::endl;
    std::cout << "Color fy: " << intrinsics->color_params.fy << std::endl;
    std::cout << "Color cx: " << intrinsics->color_params.cx << std::endl;
    std::cout << "Color cy: " << intrinsics->color_params.cy << std::endl;
    std::cout << "Color shift_d: " << intrinsics->color_params.shift_d << std::endl;
    std::cout << "Color shift_m: " << intrinsics->color_params.shift_m << std::endl;
    std::cout << "Color mx_x3y0: " << intrinsics->color_params.mx_x3y0 << std::endl;
    std::cout << "Color mx_x0y3: " << intrinsics->color_params.mx_x0y3 << std::endl;
    std::cout << "Color mx_x2y1: " << intrinsics->color_params.mx_x2y1 << std::endl;
    std::cout << "Color mx_x1y2: " << intrinsics->color_params.mx_x1y2 << std::endl;
    std::cout << "Color mx_x2y0: " << intrinsics->color_params.mx_x2y0 << std::endl;
    std::cout << "Color mx_x0y2: " << intrinsics->color_params.mx_x0y2 << std::endl;
    std::cout << "Color mx_x1y1: " << intrinsics->color_params.mx_x1y1 << std::endl;
    std::cout << "Color mx_x1y0: " << intrinsics->color_params.mx_x1y0 << std::endl;
    std::cout << "Color mx_x0y1: " << intrinsics->color_params.mx_x0y1 << std::endl;
    std::cout << "Color mx_x0y0: " << intrinsics->color_params.mx_x0y0 << std::endl;
    std::cout << "Color my_x3y0: " << intrinsics->color_params.my_x3y0 << std::endl;
    std::cout << "Color my_x0y3: " << intrinsics->color_params.my_x0y3 << std::endl;
    std::cout << "Color my_x2y1: " << intrinsics->color_params.my_x2y1 << std::endl;
    std::cout << "Color my_x1y2: " << intrinsics->color_params.my_x1y2 << std::endl;
    std::cout << "Color my_x2y0: " << intrinsics->color_params.my_x2y0 << std::endl;
    std::cout << "Color my_x0y2: " << intrinsics->color_params.my_x0y2 << std::endl;
    std::cout << "Color my_x1y1: " << intrinsics->color_params.my_x1y1 << std::endl;
    std::cout << "Color my_x1y0: " << intrinsics->color_params.my_x1y0 << std::endl;
    std::cout << "Color my_x0y1: " << intrinsics->color_params.my_x0y1 << std::endl;
    std::cout << "Color my_x0y0: " << intrinsics->color_params.my_x0y0 << std::endl;

    std::cout << "IR fx: " << intrinsics->ir_params.fx << std::endl;
    std::cout << "IR fy: " << intrinsics->ir_params.fy << std::endl;
    std::cout << "IR cx: " << intrinsics->ir_params.cx << std::endl;
    std::cout << "IR cy: " << intrinsics->ir_params.cy << std::endl;
    std::cout << "IR k1: " << intrinsics->ir_params.k1 << std::endl;
    std::cout << "IR k2: " << intrinsics->ir_params.k2 << std::endl;
    std::cout << "IR k3: " << intrinsics->ir_params.k3 << std::endl;
    std::cout << "IR p1: " << intrinsics->ir_params.p1 << std::endl;
    std::cout << "IR p2: " << intrinsics->ir_params.p2 << std::endl;
}

void _display_azure_kinect_frames()
{
    const int TARGET_BITRATE = 2000;
    const int32_t TIMEOUT_IN_MS = 1000;

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

    if (!device->start(config)) {
        std::cout << "Failed to start the Azure Kinect." << std::endl;
        return;
    }

    auto calibration = device->getCalibration(config.depth_mode, config.color_resolution);
    if (!calibration) {
        std::cout << "Failed to receive calibration of the Azure Kinect." << std::endl;
        return;
    }

    // K4A_COLOR_RESOLUTION_720P has a resolution of 1280x720.
    Vp8Encoder encoder(calibration->color_camera_calibration.resolution_width,
                       calibration->color_camera_calibration.resolution_height,
                       TARGET_BITRATE);
    Vp8Decoder decoder;

    for (;;) {
        auto capture = device->getCapture(TIMEOUT_IN_MS);
        if (!capture) {
            std::cout << "Could not get a capture from the Azure Kinect." << std::endl;
            return;
        }

        auto color_image = capture->getColorImage();
        if (!color_image) {
            continue;
        }

        auto yuv_image = createYuvImageFromAzureKinectYuy2Buffer(color_image->getBuffer(), color_image->getWidth(), color_image->getHeight(), color_image->getStride());
        //auto vp8_frame = encoder.encode(yuv_image);
        //auto ffmpeg_frame = decoder.decode(vp8_frame.data(), vp8_frame.size());
        //auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame.av_frame()));
        auto color_mat = createCvMatFromYuvImage(yuv_image);

        cv::imshow("Color", color_mat);
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

    auto calibration = device->getCalibration(K4A_DEPTH_MODE_NFOV_UNBINNED, K4A_COLOR_RESOLUTION_1080P);
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
        // An kind of an easter egg.
        // If "intrinsics" is entered, prints KinectIntrinsics instead of displaying frames.
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