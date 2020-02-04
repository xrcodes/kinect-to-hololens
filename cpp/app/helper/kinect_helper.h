#include <optional>
#include <k4a/k4a.hpp>

namespace kh
{
// For having an interface combining devices and playbacks one day in the future...
class KinectDevice
{
private:
    KinectDevice(k4a::device&& device,
                 k4a_device_configuration_t configuration,
                 std::chrono::milliseconds timeout)
        : device_(std::move(device)), configuration_(configuration), timeout_(timeout)
    {
    }

public:
    static std::optional<KinectDevice> create(k4a_device_configuration_t configuration,
                                              std::chrono::milliseconds time_out)
    {
        try {
            auto device = k4a::device::open(K4A_DEVICE_DEFAULT);
            return KinectDevice(std::move(device), configuration, time_out);
        } catch (std::exception e) {
            printf("Error opening k4a::device in KinectDevice: %s\n", e.what());
            return std::nullopt;
        }
    }

    void start()
    {
        device_.start_cameras(&configuration_);
    }

    k4a::calibration getCalibration()
    {
        return device_.get_calibration(configuration_.depth_mode,
                                       configuration_.color_resolution);
    }

    std::optional<k4a::capture> getCapture()
    {
        k4a::capture capture;
        if (!device_.get_capture(&capture, timeout_))
            return std::nullopt;

        return capture;
    }

private:
    k4a::device device_;
    k4a_device_configuration_t configuration_;
    std::chrono::milliseconds timeout_;
};
}