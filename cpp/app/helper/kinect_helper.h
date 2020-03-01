#include <optional>
#include <k4a/k4a.hpp>

namespace kh
{
// For having an interface combining devices and playbacks one day in the future...
class KinectDevice
{
public:
    KinectDevice(k4a_device_configuration_t configuration,
                 std::chrono::milliseconds timeout)
        : device_{k4a::device::open(K4A_DEVICE_DEFAULT)}, configuration_{configuration}, timeout_{timeout}
    {
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