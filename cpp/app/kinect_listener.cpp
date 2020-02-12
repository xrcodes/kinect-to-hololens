#include <algorithm>
#include <iostream>
#define NOMINMAX
#include <windows.h>
#include "helper/soundio_helper.h"

namespace kh
{
int main()
{
    const double MICROPHONE_LATENCY = 0.2; // seconds

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

    for (int i = 0; i < audio->getInputDeviceCount(); ++i) {
        printf("input_device[%d]: %s\n", i, audio->getInputDevice(i)->name());
    }
    for (int i = 0; i < audio->getOutputDeviceCount(); ++i) {
        printf("output_device[%d]: %s\n", i, audio->getOutputDevice(i)->name());
    }

    int in_device_index = -1;
    // Finding the non-raw Azure Kinect device.
    for (int i = 0; i < audio->getInputDeviceCount(); ++i) {
        auto device = audio->getInputDevice(i);
        if (!(device->is_raw())) {
            if (strcmp(device->name(), "Microphone Array (Azure Kinect Microphone Array)") == 0) {
                in_device_index = i;
                break;
            }
        }
    }
    if (in_device_index < 0) {
        printf("Could not find an Azure Kinect...\n");
        return 1;
    }

    int default_out_device_index = audio->getDefaultOutputDeviceIndex();
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

    auto in_stream = AudioInStream::create(*in_device);
    if (!in_stream) {
        printf("out of memory\n");
        return 1;
    }
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    const int K4AMicrophoneSampleRate = 48000;
    //in_stream->set_format(SoundIoFormatFloat32LE);
    in_stream->set_format(SoundIoFormatS16LE);
    in_stream->set_sample_rate(K4AMicrophoneSampleRate);
    in_stream->set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0));
    in_stream->set_software_latency(MICROPHONE_LATENCY);
    in_stream->set_read_callback(soundio_helper::azure_kinect_read_callback);
    in_stream->set_overflow_callback(soundio_helper::overflow_callback);
    if (err = in_stream->open()) {
        printf("unable to open input stream: %s\n", soundio_strerror(err));
        return 1;
    }

    auto out_stream = AudioOutStream::create(*out_device);
    if (!out_stream) {
        printf("out of memory\n");
        return 1;
    }
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    //out_stream->set_format(SoundIoFormatFloat32LE);
    out_stream->set_format(SoundIoFormatS16LE);
    out_stream->set_sample_rate(K4AMicrophoneSampleRate);
    out_stream->set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo));
    out_stream->set_software_latency(MICROPHONE_LATENCY);
    out_stream->set_write_callback(soundio_helper::write_callback);
    out_stream->set_underflow_callback(soundio_helper::underflow_callback);
    if (err = out_stream->open()) {
        printf("unable to open output stream: %s\n", soundio_strerror(err));
        return 1;
    }

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    const int STEREO_CHANNEL_COUNT = 2;
    int capacity = MICROPHONE_LATENCY * 2 * in_stream->sample_rate() * in_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    soundio_helper::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!soundio_helper::ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }
    char* buf = soundio_ring_buffer_write_ptr(soundio_helper::ring_buffer);
    int fill_count = MICROPHONE_LATENCY * out_stream->sample_rate() * out_stream->bytes_per_frame();
    memset(buf, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(soundio_helper::ring_buffer, fill_count);
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