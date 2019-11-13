#include "azure_kinect.h"

#include <iostream>

namespace azure_kinect
{
std::unique_ptr<AzureKinectDevice> obtainAzureKinectDevice()
{
    k4a_device_t device = nullptr;
    if (k4a_device_open(K4A_DEVICE_DEFAULT, &device) != K4A_RESULT_SUCCEEDED) {
        std::cout << "Failed to open an Azure Kinect." << std::endl;
        return nullptr;
    }

    return std::make_unique<AzureKinectDevice>(device);
}

k4a_device_configuration_t getDefaultDeviceConfiguration()
{
    k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_YUY2;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;

    return configuration;
}
}