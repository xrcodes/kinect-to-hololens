#include <algorithm>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <gsl/gsl>
#include "helper/soundio_helper.h"

namespace kh
{
int main()
{
    constexpr int K4AMicrophoneSampleRate{48000};
    constexpr double MICROPHONE_LATENCY{0.2}; // seconds
    constexpr int STEREO_CHANNEL_COUNT{2};

    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    auto default_speaker{audio.getDefaultOutputDevice()};

    AudioInStream kinect_microphone_stream(kinect_microphone);
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream.set_format(SoundIoFormatFloat32LE);
    kinect_microphone_stream.set_sample_rate(K4AMicrophoneSampleRate);
    kinect_microphone_stream.set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0));
    kinect_microphone_stream.set_software_latency(MICROPHONE_LATENCY);
    kinect_microphone_stream.set_read_callback(soundio_helper::azure_kinect_read_callback);
    kinect_microphone_stream.set_overflow_callback(soundio_helper::overflow_callback);

    if (int error = kinect_microphone_stream.open()) {
        printf("unable to open input stream: %s\n", soundio_strerror(error));
        return 1;
    }

    AudioOutStream default_speaker_stream(default_speaker);
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    default_speaker_stream.set_format(SoundIoFormatFloat32LE);
    default_speaker_stream.set_sample_rate(K4AMicrophoneSampleRate);
    default_speaker_stream.set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo));
    default_speaker_stream.set_software_latency(MICROPHONE_LATENCY);
    default_speaker_stream.set_write_callback(soundio_helper::write_callback);
    default_speaker_stream.set_underflow_callback(soundio_helper::underflow_callback);

    if (int error = default_speaker_stream.open()) {
        printf("unable to open output stream: %s\n", soundio_strerror(error));
        return 1;
    }

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    int capacity{gsl::narrow_cast<int>(MICROPHONE_LATENCY * 2 * kinect_microphone_stream.sample_rate() * kinect_microphone_stream.bytes_per_sample() * STEREO_CHANNEL_COUNT)};
    //soundio_helper::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    AudioRingBuffer ring_buffer{audio, capacity};
    soundio_helper::ring_buffer = ring_buffer.get();

    char* write_ptr = soundio_ring_buffer_write_ptr(soundio_helper::ring_buffer);
    int fill_count = MICROPHONE_LATENCY * default_speaker_stream.sample_rate() * default_speaker_stream.bytes_per_frame();
    memset(write_ptr, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(soundio_helper::ring_buffer, fill_count);

    if (int error = kinect_microphone_stream.start()) {
        printf("unable to start input device: %s\n", soundio_strerror(error));
        return 1;
    }
    if (int error = default_speaker_stream.start()) {
        printf("unable to start output device: %s\n", soundio_strerror(error));
        return 1;
    }

    printf("start for loop\n");
    for (;;) {
        audio.flushEvents();
        Sleep(1);
    }
    return 0;
}
}

int main()
{
    return kh::main();
}