#include <iostream>
#include "helper/opencv_helper.h"
#include "kh_depth_compression_helper.h"
#include "azure_kinect/azure_kinect.h"

namespace kh
{
void _display_frames(DepthCompressionType type)
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

    int depth_frame_width = calibration->depth_camera_calibration.resolution_width;
    int depth_frame_height = calibration->depth_camera_calibration.resolution_height;
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
        auto depth_encoder_frame = depth_encoder->encode(reinterpret_cast<short*>(depth_image->getBuffer()));
        auto depth_pixels = depth_decoder->decode(depth_encoder_frame.data(), depth_encoder_frame.size());
        auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_pixels.data()), depth_image->getWidth(), depth_image->getHeight());

        // Displays the color and depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;
    }
}

void _display_calibration()
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
        std::cout << "Enter depth compression type (RVL: 1, TRVL: 2, VP8: 3): ";
        std::string line;
        std::getline(std::cin, line);

        // If "calibration" is entered, prints calibration information instead of displaying frames.
        if (line == "calibration") {
            _display_calibration();
            continue;
        }

        if (line == "1") {
            _display_frames(DepthCompressionType::Rvl);
        } else if (line == "2") {
            _display_frames(DepthCompressionType::Trvl);
        } else if (line == "3") {
            _display_frames(DepthCompressionType::Vp8);
        } else {
            // Use TRVL as the default type.
            _display_frames(DepthCompressionType::Trvl);
        }

    }
}
}

int main()
{
    kh::display_frames();
    return 0;
}