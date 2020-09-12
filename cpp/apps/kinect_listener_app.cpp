#include <algorithm>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#undef NOMINMAX
#include <gsl/gsl>
#include "utils/soundio_utils.h"

namespace kh
{
int start()
{
    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    auto default_speaker{audio.getDefaultOutputDevice()};

    AudioInStream kinect_microphone_stream{create_kinect_microphone_stream(audio)};
    AudioOutStream default_speaker_stream{create_default_speaker_stream(audio)};

    constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

    soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    if (!soundio_callback::ring_buffer)
        throw std::runtime_error("Failed in soundio_ring_buffer_create()...");
    
    char* write_ptr{soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer)};
    constexpr int fill_count{gsl::narrow<int>(KH_LATENCY_SECONDS * KH_BYTES_PER_SECOND)};
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
    return kh::start();
}