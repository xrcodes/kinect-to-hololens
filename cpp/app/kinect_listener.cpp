#include <algorithm>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <gsl/gsl>
#include "helper/soundio_callback.h"

namespace kh
{
int main()
{
    constexpr int KINECT_MICROPHONE_SAMPLE_RATE{48000};
    constexpr int STEREO_CHANNEL_COUNT{2};
    constexpr double MICROPHONE_LATENCY{0.2}; // seconds

    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    auto default_speaker{audio.getDefaultOutputDevice()};

    AudioInStream kinect_microphone_stream(kinect_microphone);
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream.get()->format = SoundIoFormatFloat32LE;
    kinect_microphone_stream.get()->sample_rate = KINECT_MICROPHONE_SAMPLE_RATE;
    kinect_microphone_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    kinect_microphone_stream.get()->software_latency = MICROPHONE_LATENCY;
    kinect_microphone_stream.get()->read_callback = soundio_callback::azure_kinect_read_callback;
    kinect_microphone_stream.get()->overflow_callback = soundio_callback::overflow_callback;
    kinect_microphone_stream.open();

    AudioOutStream default_speaker_stream(default_speaker);
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    default_speaker_stream.get()->format = SoundIoFormatFloat32LE;
    default_speaker_stream.get()->sample_rate = KINECT_MICROPHONE_SAMPLE_RATE;
    default_speaker_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
    default_speaker_stream.get()->software_latency = MICROPHONE_LATENCY;
    default_speaker_stream.get()->write_callback = soundio_callback::write_callback;
    default_speaker_stream.get()->underflow_callback = soundio_callback::underflow_callback;
    default_speaker_stream.open();

    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    // Therefore, we use bytes_per_sample * 2 instead of bytes_per_frame.
    const int bytes_per_second{kinect_microphone_stream.get()->sample_rate * kinect_microphone_stream.get()->bytes_per_sample * STEREO_CHANNEL_COUNT};
    const int capacity{gsl::narrow_cast<int>(MICROPHONE_LATENCY * 2 * bytes_per_second)};

    soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    if (!soundio_callback::ring_buffer) {
        printf("unable to create ring buffer\n");
        return 1;
    }
    
    char* write_ptr{soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer)};
    const int fill_count{gsl::narrow_cast<int>(MICROPHONE_LATENCY * bytes_per_second)};
    memset(write_ptr, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(soundio_callback::ring_buffer, fill_count);

    kinect_microphone_stream.start();
    default_speaker_stream.start();

    printf("start for loop\n");
    for (;;) {
        soundio_flush_events(audio.get());
        Sleep(1);
    }
    return 0;
}
}

int main()
{
    return kh::main();
}