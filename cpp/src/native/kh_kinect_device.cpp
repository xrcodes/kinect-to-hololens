#include "kh_kinect_device.h"

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
KinectFrame::KinectFrame(k4a::image&& color_image, k4a::image&& depth_image, k4a_imu_sample_t imu_sample)
    : color_image_{color_image}, depth_image_{depth_image}, imu_sample_{imu_sample}
{
}

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
}