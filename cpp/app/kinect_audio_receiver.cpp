#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include "helper/soundio_helper.h"
#include "kh_receiver.h"

namespace kh
{
int main(std::string ip_address, int port)
{
    const int AZURE_KINECT_SAMPLE_RATE = 48000;
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

    int default_out_device_index = audio->getDefaultOutputDeviceIndex();
    if (default_out_device_index < 0) {
        printf("no output device found\n");
        return 1;
    }

    auto out_device = audio->getOutputDevice(default_out_device_index);
    if (!out_device) {
        printf("could not get output device: out of memory\n");
        return 1;
    }

    auto out_stream = AudioOutStream::create(*out_device);
    if (!out_stream) {
        printf("out of memory\n");
        return 1;
    }
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    out_stream->set_format(SoundIoFormatS16LE);
    out_stream->set_sample_rate(AZURE_KINECT_SAMPLE_RATE);
    out_stream->set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo));
    out_stream->set_software_latency(MICROPHONE_LATENCY);
    out_stream->set_write_callback(libsoundio::helper::write_callback);
    out_stream->set_underflow_callback(libsoundio::helper::underflow_callback);
    if (err = out_stream->open()) {
        printf("unable to open output stream: %s\n", soundio_strerror(err));
        return 1;
    }

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    const int STEREO_CHANNEL_COUNT = 2;
    //int capacity = MICROPHONE_LATENCY * 2 * in_stream->sample_rate() * in_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    int capacity = MICROPHONE_LATENCY * 2 * out_stream->sample_rate() * out_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    libsoundio::helper::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!libsoundio::helper::ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }
    char* buf = soundio_ring_buffer_write_ptr(libsoundio::helper::ring_buffer);
    int fill_count = MICROPHONE_LATENCY * out_stream->sample_rate() * out_stream->bytes_per_frame();
    memset(buf, 0, fill_count);
    soundio_ring_buffer_advance_write_ptr(libsoundio::helper::ring_buffer, fill_count);

    if (err = out_stream->start()) {
        printf("unable to start output device: %s\n", soundio_strerror(err));
        return 1;
    }

    asio::io_context io_context;
    Receiver receiver(io_context, 1024 * 1024);
    receiver.ping(ip_address, port);

    printf("start for loop\n");
    int sent_byte_count = 0;
    auto summary_time = std::chrono::steady_clock::now();
    for (;;) {
        audio->flushEvents();
        char* write_ptr = soundio_ring_buffer_write_ptr(libsoundio::helper::ring_buffer);
        int free_bytes = soundio_ring_buffer_free_count(libsoundio::helper::ring_buffer);
        int left_bytes = free_bytes;

        int cursor = 0;
        std::error_code error;
        while(left_bytes > 0) {
            auto packet = receiver.receive(error);

            if (!packet)
                break;

            memcpy(write_ptr + cursor, packet->data(), packet->size());

            cursor += packet->size();
            left_bytes -= packet->size();
        }
        //int fill_bytes = soundio_ring_buffer_fill_count(libsoundio::helper::ring_buffer);
        //printf("free_bytes: %d, fill_bytes: %d\n", free_bytes, fill_bytes);

        soundio_ring_buffer_advance_write_ptr(libsoundio::helper::ring_buffer, cursor);

        sent_byte_count += cursor;
        auto summary_diff = std::chrono::steady_clock::now() - summary_time;
        if (summary_diff > std::chrono::seconds(5))
        {
            printf("Bandwidth: %f Mbps\n", (sent_byte_count / (1024.0f * 1024.0f / 8.0f)) / (summary_diff.count() / 1000000000.0f));
            sent_byte_count = 0;
            summary_time = std::chrono::steady_clock::now();
        }
    }
    return 0;
}
}

int main()
{
    return kh::main("127.0.0.1", 7777);
}