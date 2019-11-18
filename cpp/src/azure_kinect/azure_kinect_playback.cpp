#include "azure_kinect.h"

#include <iostream>

namespace azure_kinect
{
AzureKinectPlayback::AzureKinectPlayback(k4a_playback_t playback)
    : playback_(playback)
{
}

AzureKinectPlayback::~AzureKinectPlayback()
{
    k4a_playback_close(playback_);
}

std::optional<k4a_calibration_t> AzureKinectPlayback::getCalibration()
{
    k4a_calibration_t calibration;
    if (k4a_playback_get_calibration(playback_, &calibration) != K4A_RESULT_SUCCEEDED) {
        std::cout << "Could not find calibration information from playback." << std::endl;
        return std::nullopt;
    }

    return calibration;
}

std::unique_ptr<AzureKinectCapture> AzureKinectPlayback::getNextCapture()
{
    k4a_capture_t capture = nullptr;
    if (k4a_playback_get_next_capture(playback_, &capture) != K4A_RESULT_SUCCEEDED) {
        std::cout << "Cannot find capture from playback." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectCapture>(capture);
}
}