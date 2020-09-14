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
    auto sound_io{create_sound_io_handle()};
    auto kinect_microphone{find_kinect_microphone(sound_io)};
    auto default_speaker{get_sound_io_default_output_device(sound_io)};

    auto kinect_microphone_stream{create_kinect_microphone_stream(sound_io, soundio_callback::kinect_microphone_read_callback, soundio_callback::overflow_callback)};
    auto default_speaker_stream{create_default_speaker_stream(sound_io, soundio_callback::write_callback, soundio_callback::underflow_callback)};

    constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

    soundio_callback::ring_buffer_ = soundio_ring_buffer_create(sound_io.get(), capacity);
    if (!soundio_callback::ring_buffer_)
        throw std::runtime_error("Failed in soundio_ring_buffer_create()...");
    
    char* write_ptr{soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer_)};
    constexpr int fill_count{gsl::narrow<int>(KH_LATENCY_SECONDS * KH_BYTES_PER_SECOND)};
    memset(write_ptr, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(soundio_callback::ring_buffer_, fill_count);

    if (int error = soundio_instream_start(kinect_microphone_stream.get()))
        throw std::runtime_error(std::string("Failed to start AudioInStream: ") + std::to_string(error));

    if (int error = soundio_outstream_start(default_speaker_stream.get()))
        throw std::runtime_error(std::string("Failed to start AudioOutStream: ") + std::to_string(error));

    printf("start for loop\n");
    for (;;) {
        soundio_flush_events(sound_io.get());
        Sleep(1);
    }
    return 0;
}
}

int main()
{
    return kh::start();
}