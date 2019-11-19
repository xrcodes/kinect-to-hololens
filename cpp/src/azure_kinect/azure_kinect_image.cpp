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

k4a_image_format_t AzureKinectImage::getFormat()
{
    return k4a_image_get_format(image_);
}

size_t AzureKinectImage::getSize()
{
    return k4a_image_get_size(image_);
}
}