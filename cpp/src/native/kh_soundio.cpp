#include "kh_soundio.h"

namespace kh
{
SoundIoHandle create_sound_io_handle()
{
    auto sound_io{soundio_create()};
    if (!sound_io)
        throw std::runtime_error("Null sound_io in create_sound_io_handle().");

    int err = soundio_connect(sound_io);
    if (err) {
        throw std::runtime_error("soundio_connect failed in create_sound_io_handle().");
    }

    soundio_flush_events(sound_io);

    return {sound_io, &soundio_destroy};
}

std::vector<SoundIoDeviceHandle> get_sound_io_input_devices(const SoundIoHandle& sound_io)
{
    int input_device_count{soundio_input_device_count(sound_io.get())};
    std::vector<SoundIoDeviceHandle> input_devices;
    for (int i = 0; i < input_device_count; ++i)
    {
        auto device_ptr{soundio_get_input_device(sound_io.get(), i)};
        if (!device_ptr)
            printf("Failed to get Input Devices...");

        input_devices.push_back({device_ptr, &soundio_device_unref});
    }

    return input_devices;
}

SoundIoDeviceHandle get_sound_io_default_output_device(const SoundIoHandle& sound_io)
{
    auto device_ptr{soundio_get_output_device(sound_io.get(), soundio_default_output_device_index(sound_io.get()))};
    if (!device_ptr)
        printf("Failed to get the Default Output Device...");

    return SoundIoDeviceHandle(device_ptr, &soundio_device_unref);
}

SoundIoInStreamHandle create_sound_io_instream(const SoundIoDeviceHandle& device)
{
    return {soundio_instream_create(device.get()), &soundio_instream_destroy};
}

SoundIoOutStreamHandle create_sound_io_outstream(const SoundIoDeviceHandle& device)
{
    return {soundio_outstream_create(device.get()), &soundio_outstream_destroy};
}

SoundIoDeviceHandle find_kinect_microphone(const SoundIoHandle& sound_io)
{
    auto input_devices{get_sound_io_input_devices(sound_io)};
    for (auto& input_device : input_devices) {
        if (!input_device.get()->is_raw) {
            std::string device_name{input_device.get()->name};
            if (device_name.find("Azure Kinect Microphone Array") != device_name.npos)
                return std::move(input_device);
        }
    }

    throw std::runtime_error("Could not find a Kinect Microphone...");
}
}