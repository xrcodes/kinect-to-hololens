#include "kh_kinect.h"

#include <iostream>

namespace
{
k4a_device_configuration_t get_default_configuration()
{
    k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
    return configuration;
}
}

namespace kh
{
KinectDevice::KinectDevice(k4a_device_configuration_t configuration, std::chrono::milliseconds timeout)
    : device_{k4a::device::open(K4A_DEVICE_DEFAULT)}, configuration_{configuration}, timeout_{timeout}
{
}

KinectDevice::KinectDevice() : KinectDevice(get_default_configuration(), std::chrono::milliseconds(1000))
{
}

void KinectDevice::start()
{
    device_.start_cameras(&configuration_);
    device_.start_imu();
}

bool KinectDevice::isDevice()
{
    return true;
}

k4a::calibration KinectDevice::getCalibration()
{
    return device_.get_calibration(configuration_.depth_mode,
                                   configuration_.color_resolution);
}

std::optional<KinectFrame> KinectDevice::getFrame()
{
    k4a::capture capture;
    if (!device_.get_capture(&capture, timeout_))
        return std::nullopt;

    auto color_image{capture.get_color_image()};
    if (!color_image)
        return std::nullopt;

    auto depth_image{capture.get_depth_image()};
    if (!depth_image)
        return std::nullopt;

    k4a_imu_sample_t imu_sample;
    if (!device_.get_imu_sample(&imu_sample, timeout_))
        return std::nullopt;

    return KinectFrame{std::move(color_image), std::move(depth_image), std::move(imu_sample)};
}

KinectPlayback::KinectPlayback(const std::string& path)
    : playback_(k4a::playback::open(path.c_str())), path_(path)
{
    playback_.set_color_conversion(K4A_IMAGE_FORMAT_COLOR_BGRA32);
}

void KinectPlayback::reset()
{
    playback_ = k4a::playback::open(path_.c_str());
    playback_.set_color_conversion(K4A_IMAGE_FORMAT_COLOR_BGRA32);
}

bool KinectPlayback::isDevice()
{
    return false;
}

k4a::calibration KinectPlayback::getCalibration()
{
    return playback_.get_calibration();
}

std::optional<KinectFrame> KinectPlayback::getFrame()
{
    k4a::capture capture;
    // get_next_capture() returns false at EOF.
    if (!playback_.get_next_capture(&capture)) {
        reset();
        if (!playback_.get_next_capture(&capture))
            throw std::runtime_error("KinectPlayback has no frame inside.");
    }

    auto color_image{capture.get_color_image()};
    if (!color_image)
        return std::nullopt;

    auto depth_image{capture.get_depth_image()};
    if (!depth_image)
        return std::nullopt;

    k4a_imu_sample_t imu_sample;
    if (!playback_.get_next_imu_sample(&imu_sample))
        return std::nullopt;

    return KinectFrame{std::move(color_image), std::move(depth_image), std::move(imu_sample)};
}
}