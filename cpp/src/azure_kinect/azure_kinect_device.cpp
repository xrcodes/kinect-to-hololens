#include "azure_kinect.h"

#include <iostream>

namespace azure_kinect
{
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
}