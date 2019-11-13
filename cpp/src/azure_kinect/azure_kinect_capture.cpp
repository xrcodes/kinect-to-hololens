#include "azure_kinect.h"

#include <iostream>

namespace azure_kinect
{
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
}