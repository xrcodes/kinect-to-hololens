#include "soundio_utils.h"

#include <iostream>

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
            std::cout << "Failed to get Input Devices...\n";

        input_devices.push_back({device_ptr, &soundio_device_unref});
    }

    return input_devices;
}

SoundIoDeviceHandle get_sound_io_default_output_device(const SoundIoHandle& sound_io)
{
    auto device_ptr{soundio_get_output_device(sound_io.get(), soundio_default_output_device_index(sound_io.get()))};
    if (!device_ptr)
        std::cout << "Failed to get the Default Output Device...\n";

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
        if (!input_device->is_raw) {
            std::string device_name{input_device->name};
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
    const int kinect_microphone_bytes_per_second{kinect_microphone_stream->sample_rate * kinect_microphone_stream->bytes_per_sample * KH_CHANNEL_COUNT};
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

    const int default_speaker_bytes_per_second{default_speaker_stream->sample_rate * default_speaker_stream->bytes_per_frame};
    if (KH_BYTES_PER_SECOND != default_speaker_bytes_per_second)
        throw std::runtime_error("KH_BYTES_PER_SECOND != default_speaker_bytes_per_second");

    return default_speaker_stream;
}

void write_instream_to_buffer(SoundIoInStream* instream, int frame_count_min, int frame_count_max, SoundIoRingBuffer* ring_buffer, int channel_count)
{
    SoundIoChannelArea* areas;
    int err;
    char* write_ptr = soundio_ring_buffer_write_ptr(ring_buffer);
    int free_bytes = soundio_ring_buffer_free_count(ring_buffer);

    // Using only the first two channels of Azure Kinect...
    // Usage of bytes_per_frame based on the input channel_count instead of
    // instream->bytes_per_frame based on the number of channels detected from the device
    // allows not using all instream channels of the device.
    // This is important for Azure Kinect as it has 7 instreams and one way of converting it to a stereo instream
    // is using only the first two channels of it.
    int bytes_per_frame = instream->bytes_per_sample * channel_count;
    int free_count = free_bytes / bytes_per_frame;

    if (frame_count_min > free_count) {
        std::cout << "ring buffer overflow\n";
        return;
    }

    int write_frames = std::min<int>(free_count, frame_count_max);
    int frames_left = write_frames;
    for (;;) {
        int frame_count = frames_left;

        if ((err = soundio_instream_begin_read(instream, &areas, &frame_count))) {
            std::cout << "begin read error: " << soundio_strerror(err) << std::endl;
            abort();
        }

        if (!frame_count)
            break;

        if (!areas) {
            // Due to an overflow there is a hole. Fill the ring buffer with
            // silence for the size of the hole.
            memset(write_ptr, 0, static_cast<long long>(frame_count) * bytes_per_frame);
            std::cout << "Dropped " << frame_count << " frames due to internal overflow\n";
        } else {
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < KH_CHANNEL_COUNT; ch += 1) {
                    memcpy(write_ptr, areas[ch].ptr, instream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                    write_ptr += instream->bytes_per_sample;
                }
            }
        }

        if ((err = soundio_instream_end_read(instream))) {
            std::cout << "end read error: " << soundio_strerror(err);
            abort();
        }

        frames_left -= frame_count;
        if (frames_left <= 0)
            break;
    }

    int advance_bytes = write_frames * bytes_per_frame;
    soundio_ring_buffer_advance_write_ptr(ring_buffer, advance_bytes);
}

void write_buffer_to_outstream(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max, SoundIoRingBuffer* ring_buffer)
{
    struct SoundIoChannelArea* areas;
    int frames_left;
    int frame_count;
    int err;

    char* read_ptr = soundio_ring_buffer_read_ptr(ring_buffer);
    int fill_bytes = soundio_ring_buffer_fill_count(ring_buffer);
    int fill_count = fill_bytes / outstream->bytes_per_frame;

    if (frame_count_min > fill_count) {
        // Ring buffer does not have enough data, fill with zeroes.
        frames_left = frame_count_min;
        for (;;) {
            frame_count = frames_left;
            if (frame_count <= 0)
                return;
            if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
                std::cout << "begin write error: " << soundio_strerror(err);
                abort();
            }
            if (frame_count <= 0)
                return;
            for (int frame = 0; frame < frame_count; frame += 1) {
                for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                    memset(areas[ch].ptr, 0, outstream->bytes_per_sample);
                    areas[ch].ptr += areas[ch].step;
                }
            }
            if ((err = soundio_outstream_end_write(outstream))) {
                std::cout << "end write error: " << soundio_strerror(err);
                abort();
            }
            frames_left -= frame_count;
        }
    }

    int read_count = std::min<int>(frame_count_max, fill_count);
    frames_left = read_count;

    while (frames_left > 0) {
        int frame_count = frames_left;

        if ((err = soundio_outstream_begin_write(outstream, &areas, &frame_count))) {
            std::cout << "begin write error: " << soundio_strerror(err);
            abort();
        }

        if (frame_count <= 0)
            break;

        for (int frame = 0; frame < frame_count; frame += 1) {
            for (int ch = 0; ch < outstream->layout.channel_count; ch += 1) {
                memcpy(areas[ch].ptr, read_ptr, outstream->bytes_per_sample);
                areas[ch].ptr += areas[ch].step;
                read_ptr += outstream->bytes_per_sample;
            }
        }

        if ((err = soundio_outstream_end_write(outstream))) {
            std::cout << "end write error: " << soundio_strerror(err);
            abort();
        }

        frames_left -= frame_count;
    }

    soundio_ring_buffer_advance_read_ptr(ring_buffer, read_count * outstream->bytes_per_frame);
}
}
