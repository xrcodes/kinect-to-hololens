/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */
#include <algorithm>
#include <soundio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <optional>
#include <thread>
#include <windows.h>
#include <iostream>
#include "helper/soundio_helper.h"

namespace kh
{
static enum SoundIoFormat prioritized_formats[] = {
    SoundIoFormatFloat32NE,
    SoundIoFormatFloat32FE,
    SoundIoFormatS32NE,
    SoundIoFormatS32FE,
    SoundIoFormatS24NE,
    SoundIoFormatS24FE,
    SoundIoFormatS16NE,
    SoundIoFormatS16FE,
    SoundIoFormatFloat64NE,
    SoundIoFormatFloat64FE,
    SoundIoFormatU32NE,
    SoundIoFormatU32FE,
    SoundIoFormatU24NE,
    SoundIoFormatU24FE,
    SoundIoFormatU16NE,
    SoundIoFormatU16FE,
    SoundIoFormatS8,
    SoundIoFormatU8,
    SoundIoFormatInvalid,
};
static int prioritized_sample_rates[] = {
    48000,
    44100,
    96000,
    24000,
    0,
};

int main() {
    double microphone_latency = 0.2; // seconds

    auto audio = Audio::create();
    if (!audio)
        libsoundio::example::panic("out of memory");
    int err = audio->connect();
    if (err)
        libsoundio::example::panic("error connecting: %s", soundio_strerror(err));
    audio->flushEvents();
    int default_out_device_index = soundio_default_output_device_index(audio->ptr());
    if (default_out_device_index < 0)
        libsoundio::example::panic("no output device found");
    int default_in_device_index = soundio_default_input_device_index(audio->ptr());
    if (default_in_device_index < 0)
        libsoundio::example::panic("no input device found");
    auto out_device = audio->getOutputDevice(default_out_device_index);
    if (!out_device)
        libsoundio::example::panic("could not get output device: out of memory");
    auto in_device = audio->getInputDevice(default_in_device_index);
    if (!in_device)
        libsoundio::example::panic("could not get input device: out of memory");
    fprintf(stderr, "Input device: %s\n", in_device->ptr()->name);
    fprintf(stderr, "Output device: %s\n", out_device->ptr()->name);
    soundio_device_sort_channel_layouts(out_device->ptr());
    const struct SoundIoChannelLayout* layout = soundio_best_matching_channel_layout(
        out_device->ptr()->layouts, out_device->ptr()->layout_count,
        in_device->ptr()->layouts, in_device->ptr()->layout_count);
    if (!layout)
        libsoundio::example::panic("channel layouts not compatible");
    int* sample_rate;
    for (sample_rate = prioritized_sample_rates; *sample_rate; sample_rate += 1) {
        if (soundio_device_supports_sample_rate(in_device->ptr(), *sample_rate) &&
            soundio_device_supports_sample_rate(out_device->ptr(), *sample_rate))
        {
            break;
        }
    }
    if (!*sample_rate)
        libsoundio::example::panic("incompatible sample rates");
    enum SoundIoFormat* fmt;
    for (fmt = prioritized_formats; *fmt != SoundIoFormatInvalid; fmt += 1) {
        if (soundio_device_supports_format(in_device->ptr(), *fmt) &&
            soundio_device_supports_format(out_device->ptr(), *fmt))
        {
            break;
        }
    }
    if (*fmt == SoundIoFormatInvalid)
        libsoundio::example::panic("incompatible sample formats");
    //struct SoundIoInStream* instream = soundio_instream_create(in_device->device());
    auto in_stream = AudioInStream::create(*in_device);
    if (!in_stream)
        libsoundio::example::panic("out of memory");
    in_stream->ptr()->format = *fmt;
    in_stream->ptr()->sample_rate = *sample_rate;
    in_stream->ptr()->layout = *layout;
    in_stream->ptr()->software_latency = microphone_latency;
    in_stream->ptr()->read_callback = libsoundio::example::read_callback;
    if ((err = soundio_instream_open(in_stream->ptr()))) {
        fprintf(stderr, "unable to open input stream: %s", soundio_strerror(err));
        return 1;
    }
    //struct SoundIoOutStream* outstream = soundio_outstream_create(out_device->device());
    auto out_stream = AudioOutStream::create(*out_device);
    if (!out_stream)
        libsoundio::example::panic("out of memory");
    out_stream->ptr()->format = *fmt;
    out_stream->ptr()->sample_rate = *sample_rate;
    out_stream->ptr()->layout = *layout;
    out_stream->ptr()->software_latency = microphone_latency;
    out_stream->ptr()->write_callback = libsoundio::example::write_callback;
    out_stream->ptr()->underflow_callback = libsoundio::example::underflow_callback;
    if ((err = soundio_outstream_open(out_stream->ptr()))) {
        fprintf(stderr, "unable to open output stream: %s", soundio_strerror(err));
        return 1;
    }
    int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    libsoundio::example::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!libsoundio::example::ring_buffer)
        libsoundio::example::panic("unable to create ring buffer: out of memory");
    char* buf = soundio_ring_buffer_write_ptr(libsoundio::example::ring_buffer);
    int fill_count = microphone_latency * out_stream->ptr()->sample_rate * out_stream->ptr()->bytes_per_frame;
    memset(buf, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(libsoundio::example::ring_buffer, fill_count);
    if ((err = soundio_instream_start(in_stream->ptr())))
        libsoundio::example::panic("unable to start input device: %s", soundio_strerror(err));
    if ((err = soundio_outstream_start(out_stream->ptr())))
        libsoundio::example::panic("unable to start output device: %s", soundio_strerror(err));
    for (;;) {
        //soundio_wait_events is not what calls the io callbacks.
        soundio_wait_events(audio->ptr());
        //Sleep(1);
    }
    return 0;
}
}

int main()
{
    return kh::main();
}