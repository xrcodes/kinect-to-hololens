#include <optional>
#include <k4a/k4a.hpp>

namespace
{
k4a_device_configuration_t getDefaultKinectConfiguration()
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
class KinectFrame
{
public:
    KinectFrame(k4a::image&& color_image, k4a::image&& depth_image, k4a_imu_sample_t imu_sample)
        : color_image_{color_image}, depth_image_{depth_image}, imu_sample_{imu_sample}
    {
    }

    k4a::image& color_image() { return color_image_; }
    k4a::image& depth_image() { return depth_image_; }
    k4a_imu_sample_t& imu_sample() { return imu_sample_; }

private:
    k4a::image color_image_;
    k4a::image depth_image_;
    k4a_imu_sample_t imu_sample_;
};

// For having an interface combining devices and playbacks one day in the future...
class KinectDevice
{
public:

    KinectDevice(k4a_device_configuration_t configuration, std::chrono::milliseconds timeout)
        : device_{k4a::device::open(K4A_DEVICE_DEFAULT)}, configuration_{configuration}, timeout_{timeout}
    {
    }

    KinectDevice() : KinectDevice(getDefaultKinectConfiguration(), std::chrono::milliseconds(1000))
    {
    }

    void start()
    {
        device_.start_cameras(&configuration_);
        device_.start_imu();
    }

    k4a::calibration getCalibration()
    {
        return device_.get_calibration(configuration_.depth_mode,
                                       configuration_.color_resolution);
    }

    std::optional<KinectFrame> getFrame()
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

private:
    k4a::device device_;
    k4a_device_configuration_t configuration_;
    std::chrono::milliseconds timeout_;
};
}