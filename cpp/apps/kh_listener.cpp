#include <algorithm>
#include <iostream>
#include <gsl/gsl>
#include "native/kh_native.h"
#include "win32/soundio_utils.h"

namespace kh
{
static SoundIoRingBuffer* ring_buffer{nullptr};

void read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
{
    // Using KH_CHANNEL_COUNT (i.e., 2) to pick the first two channels of the Azure Kinect microphone.
    // Use (instream->bytes_per_frame / instream->bytes_per_sample) as channel_count for full usage of channels.
    write_instream_to_buffer(instream, frame_count_min, frame_count_max, ring_buffer, KH_CHANNEL_COUNT);
}

void overflow_callback(struct SoundIoInStream* instream) {
    static int count = 0;
    printf("overflow %d\n", ++count);
}

void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max)
{
    write_buffer_to_outstream(outstream, frame_count_min, frame_count_max, ring_buffer);
}

void underflow_callback(struct SoundIoOutStream* outstream) {
    static int count = 0;
    // This line leaves too many logs...
    //printf("underflow %d\n", ++count);
}

int start()
{
    auto sound_io{create_sound_io_handle()};
    auto kinect_microphone{find_kinect_microphone(sound_io)};
    auto default_speaker{get_sound_io_default_output_device(sound_io)};

    auto kinect_microphone_stream{create_kinect_microphone_stream(sound_io, read_callback, overflow_callback)};
    auto default_speaker_stream{create_default_speaker_stream(sound_io, write_callback, underflow_callback)};

    constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

    ring_buffer = soundio_ring_buffer_create(sound_io.get(), capacity);
    if (!ring_buffer)
        throw std::runtime_error("Failed in soundio_ring_buffer_create()...");
    
    char* write_ptr{soundio_ring_buffer_write_ptr(ring_buffer)};
    constexpr int fill_count{gsl::narrow<int>(KH_LATENCY_SECONDS * KH_BYTES_PER_SECOND)};
    memset(write_ptr, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(ring_buffer, fill_count);

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