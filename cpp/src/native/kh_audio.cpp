#include "kh_audio.h"

namespace kh
{
Audio::Audio()
    : ptr_(soundio_create())
{
    if (!ptr_)
        throw std::exception("Failed to construct Audio...");

    int err = soundio_connect(ptr_);
    if (err) {
        throw std::exception("Failed to connect Audio...");
    }

    soundio_flush_events(ptr_);
}

Audio::Audio(Audio&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

Audio::~Audio()
{
    if (ptr_)
        soundio_destroy(ptr_);
}

std::vector<AudioDevice> Audio::getInputDevices() const
{
    int input_device_count{soundio_input_device_count(ptr_)};
    std::vector<AudioDevice> input_devices;
    for (int i = 0; i < input_device_count; ++i)
    {
        auto device_ptr{soundio_get_input_device(ptr_, i)};
        if (!device_ptr)
            printf("Failed to get Input Devices...");

        input_devices.emplace_back(device_ptr);
    }

    return input_devices;
}

AudioDevice Audio::getDefaultOutputDevice() const
{
    auto device_ptr{soundio_get_output_device(ptr_, soundio_default_output_device_index(ptr_))};
    if (!device_ptr)
        printf("Failed to get the Default Output Device...");

    return AudioDevice(device_ptr);
}

AudioDevice::AudioDevice(SoundIoDevice* ptr)
    : ptr_(ptr)
{
}

AudioDevice::AudioDevice(const AudioDevice& other)
    : ptr_(other.ptr_)
{
    soundio_device_ref(ptr_);
}

AudioDevice::AudioDevice(AudioDevice&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioDevice::~AudioDevice()
{
    if (ptr_)
        soundio_device_unref(ptr_);
}

AudioInStream::AudioInStream(AudioDevice& device)
    : ptr_(soundio_instream_create(device.get()))
{
    if (!ptr_)
        throw std::exception("Failed to construct AudioInStream...");
}

AudioInStream::AudioInStream(AudioInStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioInStream::~AudioInStream()
{
    if (ptr_)
        soundio_instream_destroy(ptr_);
}

AudioOutStream::AudioOutStream(AudioDevice& device)
    : ptr_(soundio_outstream_create(device.get()))
{
    if (!ptr_)
        throw std::exception("Failed to construct AudioOutStream...");
}

AudioOutStream::AudioOutStream(AudioOutStream&& other) noexcept
{
    ptr_ = other.ptr_;
    other.ptr_ = nullptr;
}

AudioOutStream::~AudioOutStream()
{
    if (ptr_)
        soundio_outstream_destroy(ptr_);
}

AudioDevice find_kinect_microphone(const Audio& audio)
{
    auto input_devices{audio.getInputDevices()};
    for (auto& input_device : input_devices) {
        if (!input_device.is_raw()) {
            std::string device_name{input_device.name()};
            if (device_name.find("Azure Kinect Microphone Array") != device_name.npos)
                return input_device;
        }
    }

    throw std::exception("Could not find a Kinect Microphone...");
}
}