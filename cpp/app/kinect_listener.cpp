/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of libsoundio, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */
#include <algorithm>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#include "helper/soundio_helper.h"
#include "helper/kinect_helper.h"

namespace kh
{

int main()
{
    double microphone_latency = 0.2; // seconds

    auto audio = Audio::create();
    if (!audio) {
        printf("out of memory\n");
        return 1;
    }
    int err = audio->connect();
    if (err) {
        printf("error connecting: %s\n", soundio_strerror(err));
        return 1;
    }
    audio->flushEvents();

    for (int i = 0; i < soundio_input_device_count(audio->ptr()); ++i) {
        printf("input_device[%d]: %s\n", i, audio->getInputDevice(i)->ptr()->name);
    }
    for (int i = 0; i < soundio_output_device_count(audio->ptr()); ++i) {
        printf("output_device[%d]: %s\n", i, audio->getOutputDevice(i)->ptr()->name);
    }

    int in_device_index = -1;
    // Finding the non-raw Azure Kinect device.
    for (int i = 0; i < soundio_input_device_count(audio->ptr()); ++i) {
        auto device = audio->getInputDevice(i);
        if (!(device->ptr()->is_raw)) {
            if (strcmp(device->ptr()->name, "Microphone Array (Azure Kinect Microphone Array)") == 0) {
                in_device_index = i;
                break;
            }
        }
    }

    if (in_device_index < 0) {
        printf("Could not find an Azure Kinect...\n");
        return 1;
    }

    int default_out_device_index = soundio_default_output_device_index(audio->ptr());
    if (default_out_device_index < 0) {
        printf("no output device found\n");
        return 1;
    }

    auto in_device = audio->getInputDevice(in_device_index);
    if (!in_device) {
        printf("could not get input device: out of memory\n");
        return 1;
    }
    auto out_device = audio->getOutputDevice(default_out_device_index);
    if (!out_device) {
        printf("could not get output device: out of memory\n");
        return 1;
    }
    
    //printf("in_device_index: %d\n", in_device_index);
    //printf("Input device: %s\n", in_device->ptr()->name);
    //printf("Output device: %s\n", out_device->ptr()->name);

    //soundio_device_sort_channel_layouts(out_device->ptr());
    //const SoundIoChannelLayout* layout = soundio_best_matching_channel_layout(
    //    out_device->ptr()->layouts, out_device->ptr()->layout_count,
    //    in_device->ptr()->layouts, in_device->ptr()->layout_count);
    //if (!layout) {
    //    printf("channel layouts not compatible");
    //    return 1;
    //}
    
    // This layout is from Azure-Kinect-Sensor-SDK/tools/k4aviewer/k4amicrophone.cpp.
    //const SoundIoChannelLayout* in_layout = soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    const SoundIoChannelLayout* in_layout = soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    printf("in_layout channel_count: %d\n", in_layout->channel_count);

    int* sample_rate;
    for (sample_rate = libsoundio::example::prioritized_sample_rates; *sample_rate; ++sample_rate) {
        if (soundio_device_supports_sample_rate(in_device->ptr(), *sample_rate) &&
            soundio_device_supports_sample_rate(out_device->ptr(), *sample_rate)) {
            break;
        }
    }
    if (!*sample_rate) {
        printf("incompatible sample rates\n");
        return 1;
    }

    printf("sample_rate: %d\n", *sample_rate);

    //SoundIoFormat* fmt;
    //for (fmt = libsoundio::example::prioritized_formats; *fmt != SoundIoFormatInvalid; ++fmt) {
    //    if (soundio_device_supports_format(in_device->ptr(), *fmt) &&
    //        soundio_device_supports_format(out_device->ptr(), *fmt)) {
    //        break;
    //    }
    //}
    //if (*fmt == SoundIoFormatInvalid) {
    //    printf("incompatible sample formats\n");
    //    return 1;
    //}

    //if (soundio_device_supports_format(in_device->ptr(), SoundIoFormatFloat32LE)) {
    //    printf("SoundIoFormatFloat32LE not supported by the input device...");
    //    return 1;
    //}

    //if (soundio_device_supports_format(out_device->ptr(), SoundIoFormatFloat32LE)) {
    //    printf("SoundIoFormatFloat32LE not supported by the output device...");
    //    return 1;
    //}

    auto in_stream = AudioInStream::create(*in_device);
    if (!in_stream) {
        printf("out of memory\n");
        return 1;
    }

    int test = 213;

    const int K4AMicrophoneSampleRate = 48000;

    //in_stream->ptr()->format = *fmt;
    //in_stream->ptr()->sample_rate = *sample_rate;
    in_stream->ptr()->format = SoundIoFormatFloat32LE;
    in_stream->ptr()->sample_rate = K4AMicrophoneSampleRate;
    in_stream->ptr()->layout = *in_layout;
    in_stream->ptr()->software_latency = microphone_latency;
    in_stream->ptr()->read_callback = libsoundio::example::azure_kinect_read_callback;
    if (err = in_stream->open()) {
        printf("unable to open input stream: %s\n", soundio_strerror(err));
        return 1;
    }

    auto out_stream = AudioOutStream::create(*out_device);
    if (!out_stream) {
        printf("out of memory\n");
        return 1;
    }
    out_stream->ptr()->format = SoundIoFormatFloat32LE;
    out_stream->ptr()->sample_rate = *sample_rate;
    //out_stream->ptr()->layout = *layout;
    out_stream->ptr()->software_latency = microphone_latency;
    out_stream->ptr()->write_callback = libsoundio::example::write_callback;
    out_stream->ptr()->underflow_callback = libsoundio::example::underflow_callback;
    if (err = out_stream->open()) {
        printf("unable to open output stream: %s\n", soundio_strerror(err));
        return 1;
    }

    int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    libsoundio::example::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!libsoundio::example::ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }
    char* buf = soundio_ring_buffer_write_ptr(libsoundio::example::ring_buffer);
    int fill_count = microphone_latency * out_stream->ptr()->sample_rate * out_stream->ptr()->bytes_per_frame;
    memset(buf, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(libsoundio::example::ring_buffer, fill_count);
    if (err = in_stream->start()) {
        printf("unable to start input device: %s\n", soundio_strerror(err));
        return 1;
    }
    if (err = out_stream->start()) {
        printf("unable to start output device: %s\n", soundio_strerror(err));
        return 1;
    }

    printf("start for loop\n");
    for (;;) {
        audio->flushEvents();
        Sleep(1);
    }
    return 0;
}
}

int main()
{
    return kh::main();
}