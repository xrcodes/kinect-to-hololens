#pragma once

#include <iostream>
#include <memory>
#include <optional>
#include "k4a/k4a.h"
#include "k4arecord/playback.h"

namespace azure_kinect
{
class AzureKinectImage
{
public:
    AzureKinectImage(k4a_image_t image);
    ~AzureKinectImage();
    uint8_t* getBuffer();
    int getWidth();
    int getHeight();
    int getStride();
    k4a_image_format_t getFormat();
    size_t getSize();

private:
    k4a_image_t image_;
};

class AzureKinectCapture
{
public:
    AzureKinectCapture(k4a_capture_t capture);
    ~AzureKinectCapture();
    std::unique_ptr<AzureKinectImage> getColorImage();
    std::unique_ptr<AzureKinectImage> getDepthImage();

private:
    k4a_capture_t capture_;
};

class AzureKinectDevice
{
public:
    AzureKinectDevice(k4a_device_t device);
    ~AzureKinectDevice();
    std::optional<k4a_calibration_t> getCalibration(const k4a_depth_mode_t depth_mode, const k4a_color_resolution_t color_resolution);
    bool start(const k4a_device_configuration_t& config);
    std::unique_ptr<AzureKinectCapture> getCapture(int32_t timeout_in_ms);

private:
    k4a_device_t device_;
};

class AzureKinectPlayback
{
public:
    AzureKinectPlayback(k4a_playback_t playback);
    ~AzureKinectPlayback();
    std::optional<k4a_calibration_t> getCalibration();
    std::unique_ptr<AzureKinectCapture> getNextCapture();

private:
    k4a_playback_t playback_;
};

std::unique_ptr<AzureKinectDevice> obtainAzureKinectDevice();
k4a_device_configuration_t getDefaultDeviceConfiguration();
std::unique_ptr<AzureKinectPlayback> obtainAzureKinectPlayback(const std::string& path);
}