#pragma once

#include <optional>
#pragma warning(push)
#pragma warning(disable: 26495 26812)
#include <k4arecord/playback.hpp>
#pragma warning(pop)
#include "native/tt_native.h"

namespace kh
{
struct KinectFrame
{
    tt::TimePoint time_point;
    k4a::image color_image;
    k4a::image depth_image;
    k4a_imu_sample_t imu_sample;
};

class KinectInterface
{
public:
    // To check whether there would be a microphone.
    virtual bool isDevice() = 0;
    virtual k4a::calibration getCalibration() = 0;
    virtual std::optional<KinectFrame> getFrame() = 0;
};

// For having an interface combining devices and playbacks one day in the future...
class KinectDevice : public KinectInterface
{
public:
    KinectDevice(k4a_device_configuration_t configuration, std::chrono::milliseconds timeout);
    KinectDevice();
    void start();
    bool isDevice();
    k4a::calibration getCalibration();
    std::optional<KinectFrame> getFrame();

private:
    k4a::device device_;
    k4a_device_configuration_t configuration_;
    std::chrono::milliseconds timeout_;
};

class KinectPlayback : public KinectInterface
{
public:
    KinectPlayback(const std::string& path);
    void reset();
    bool isDevice();
    k4a::calibration getCalibration();
    std::optional<KinectFrame> getFrame();

private:
    k4a::playback playback_;
    std::string path_;
};
}