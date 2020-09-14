#pragma once

#include <optional>
#include <soundio/soundio.h>
#include "native/kh_soundio.h"

namespace kh
{
// Most of the functions inside this namescae are from example/sio_microphone.c of libsoundio (https://github.com/andrewrk/libsoundio).
// The kinect_microphone_read_callback() is modification of read_callback() that uses only the first two channels
// from the kinect microphone which actually has 7 channels.
namespace soundio_callback
{
//static SoundIoRingBuffer* ring_buffer_{nullptr};

//static void read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
//{
//    write_instream_to_buffer(instream, frame_count_min, frame_count_max, ring_buffer_, instream->bytes_per_frame / instream->bytes_per_sample);
//}
//
//static void kinect_microphone_read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
//{
//    write_instream_to_buffer(instream, frame_count_min, frame_count_max, ring_buffer_, KH_CHANNEL_COUNT);
//}

//static void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max)
//{
//    write_buffer_to_outstream(outstream, frame_count_min, frame_count_max, ring_buffer_);
//}
//
//static void underflow_callback(struct SoundIoOutStream* outstream) {
//    static int count = 0;
//    // This line leaves too many logs...
//    //printf("underflow %d\n", ++count);
//}
//
//static void overflow_callback(struct SoundIoInStream* instream) {
//    static int count = 0;
//    printf("overflow %d\n", ++count);
//}
}

}