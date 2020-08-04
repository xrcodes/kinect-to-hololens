#pragma once

#include <optional>
#include <k4a/k4a.hpp>
#include <k4arecord/playback.hpp>

namespace kh
{
struct KinectFrame
{
    k4a::image color_image;
    k4a::image depth_image;
    k4a_imu_sample_t imu_sample;
};

class KinectDeviceInterface
{
public:
    virtual k4a::calibration getCalibration() = 0;
    virtual std::optional<KinectFrame> getFrame() = 0;
};

// For having an interface combining devices and playbacks one day in the future...
class KinectDevice : public KinectDeviceInterface
{
public:
    KinectDevice(k4a_device_configuration_t configuration, std::chrono::milliseconds timeout);
    KinectDevice();
    void start();
    k4a::calibration getCalibration();
    std::optional<KinectFrame> getFrame();

private:
    k4a::device device_;
    k4a_device_configuration_t configuration_;
    std::chrono::milliseconds timeout_;
};

class KinectPlayback : public KinectDeviceInterface
{
public:
    KinectPlayback(const std::string& path);
    void reset();
    k4a::calibration getCalibration();
    std::optional<KinectFrame> getFrame();

private:
    k4a::playback playback_;
    std::string path_;
};
}