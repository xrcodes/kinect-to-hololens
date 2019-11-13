#include "azure_kinect.h"

#include <iostream>

namespace azure_kinect
{
AzureKinectImage::AzureKinectImage(k4a_image_t image)
    : image_(image)
{
}

AzureKinectImage::~AzureKinectImage()
{
    k4a_image_release(image_);
}

uint8_t* AzureKinectImage::getBuffer()
{
    return k4a_image_get_buffer(image_);
}

int AzureKinectImage::getWidth()
{
    return k4a_image_get_width_pixels(image_);
}

int AzureKinectImage::getHeight()
{
    return k4a_image_get_height_pixels(image_);
}

int AzureKinectImage::getStride()
{
    return k4a_image_get_stride_bytes(image_);
}

AzureKinectCapture::AzureKinectCapture(k4a_capture_t capture)
    : capture_(capture)
{
}

AzureKinectCapture::~AzureKinectCapture()
{
    k4a_capture_release(capture_);
}

std::unique_ptr<AzureKinectImage> AzureKinectCapture::getColorImage()
{
    k4a_image_t image = k4a_capture_get_color_image(capture_);
    if (!image) {
        std::cout << "No color image from the capture." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectImage>(image);
}

std::unique_ptr<AzureKinectImage> AzureKinectCapture::getDepthImage()
{
    k4a_image_t image = k4a_capture_get_depth_image(capture_);
    if (!image) {
        std::cout << "No depth image from the capture." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectImage>(image);
}

AzureKinectDevice::AzureKinectDevice(k4a_device_t device)
    : device_(device)
{
}

AzureKinectDevice::~AzureKinectDevice()
{
    k4a_device_close(device_);
}

std::optional<k4a_calibration_t> AzureKinectDevice::getCalibration(const k4a_depth_mode_t depth_mode, const k4a_color_resolution_t color_resolution)
{
    k4a_calibration_t calibration;
    if (k4a_device_get_calibration(device_, depth_mode, color_resolution, &calibration) != K4A_RESULT_SUCCEEDED)
        return std::nullopt;

    return calibration;
}

bool AzureKinectDevice::start(const k4a_device_configuration_t& config)
{
    return k4a_device_start_cameras(device_, &config) == K4A_RESULT_SUCCEEDED;
}

std::unique_ptr<AzureKinectCapture> AzureKinectDevice::getCapture(int32_t timeout_in_ms)
{
    k4a_capture_t capture;
    auto result = k4a_device_get_capture(device_, &capture, timeout_in_ms);

    if (result == K4A_WAIT_RESULT_FAILED) {
        std::cout << "Failed to read a capture." << std::endl;
        return nullptr;
    }
    if (result == K4A_WAIT_RESULT_TIMEOUT) {
        std::cout << "Timed out waiting for a capture." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectCapture>(capture);
}

std::unique_ptr<AzureKinectDevice> obtainAzureKinectDevice()
{
    k4a_device_t device = nullptr;

    if (k4a_device_open(K4A_DEVICE_DEFAULT, &device) != K4A_RESULT_SUCCEEDED) {
        std::cout << "Failed to open Azure Kinect..." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectDevice>(device);
}
}