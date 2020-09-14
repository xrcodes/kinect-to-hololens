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

SoundIoInStreamHandle create_kinect_microphone_stream(const SoundIoHandle& sound_io,
                                                      void (*read_callback)(struct SoundIoInStream*, int frame_count_min, int frame_count_max),
                                                      void (*overflow_callback)(struct SoundIoInStream*))
{
    auto kinect_microphone_stream{create_sound_io_instream(find_kinect_microphone(sound_io))};
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream->format = SoundIoFormatFloat32LE;
    kinect_microphone_stream->sample_rate = KH_SAMPLE_RATE;
    kinect_microphone_stream->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    kinect_microphone_stream->software_latency = KH_LATENCY_SECONDS;
    kinect_microphone_stream->read_callback = read_callback;
    kinect_microphone_stream->overflow_callback = overflow_callback;

    if (int error = soundio_instream_open(kinect_microphone_stream.get()))
        throw std::runtime_error(std::string("Failed to open AudioInStream: ") + std::to_string(error));

    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    // Therefore, we use bytes_per_sample * 2 instead of bytes_per_frame.
    const int kinect_microphone_bytes_per_second{kinect_microphone_stream.get()->sample_rate * kinect_microphone_stream.get()->bytes_per_sample * KH_CHANNEL_COUNT};
    if (KH_BYTES_PER_SECOND != kinect_microphone_bytes_per_second)
        throw std::runtime_error("KH_BYTES_PER_SECOND != kinect_microphone_bytes_per_second");

    return kinect_microphone_stream;
}

SoundIoOutStreamHandle create_default_speaker_stream(const SoundIoHandle& sound_io,
                                                     void (*write_callback)(struct SoundIoOutStream*, int frame_count_min, int frame_count_max),
                                                     void (*underflow_callback)(struct SoundIoOutStream*))
{
    auto default_speaker_stream(create_sound_io_outstream(get_sound_io_default_output_device(sound_io)));
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    default_speaker_stream.get()->format = SoundIoFormatFloat32LE;
    default_speaker_stream.get()->sample_rate = KH_SAMPLE_RATE;
    default_speaker_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
    default_speaker_stream.get()->software_latency = KH_LATENCY_SECONDS;
    default_speaker_stream.get()->write_callback = write_callback;
    default_speaker_stream.get()->underflow_callback = underflow_callback;

    if (int error = soundio_outstream_open(default_speaker_stream.get()))
        throw std::runtime_error(std::string("Failed to open AudioOutStream: ") + std::to_string(error));

    const int default_speaker_bytes_per_second{default_speaker_stream.get()->sample_rate * default_speaker_stream.get()->bytes_per_frame};
    if (KH_BYTES_PER_SECOND != default_speaker_bytes_per_second)
        throw std::runtime_error("KH_BYTES_PER_SECOND != default_speaker_bytes_per_second");

    return default_speaker_stream;
}
}