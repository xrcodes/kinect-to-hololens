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
}