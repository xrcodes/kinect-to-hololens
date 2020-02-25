#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include <opus/opus.h>
#include "helper/soundio_helper.h"
#include "native/kh_sender_socket.h"
#include "native/kh_packet_helper.h"

namespace kh
{
int main(int port)
{
    const int AZURE_KINECT_SAMPLE_RATE = 48000;
    const int STEREO_CHANNEL_COUNT = 2;
    const double MICROPHONE_LATENCY = 0.2; // seconds
    const int AUDIO_FRAME_SIZE = 960;
    
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

    auto in_device = audio->getInputDevice(in_device_index);
    if (!in_device) {
        printf("could not get input device: out of memory\n");
        return 1;
    }

    auto in_stream = AudioInStream::create(*in_device);
    if (!in_stream) {
        printf("out of memory\n");
        return 1;
    }
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    in_stream->set_format(SoundIoFormatFloat32LE);
    in_stream->set_sample_rate(AZURE_KINECT_SAMPLE_RATE);
    in_stream->set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0));
    in_stream->set_software_latency(MICROPHONE_LATENCY);
    in_stream->set_read_callback(soundio_helper::azure_kinect_read_callback);
    in_stream->set_overflow_callback(soundio_helper::overflow_callback);
    if (err = in_stream->open()) {
        printf("unable to open input stream: %s\n", soundio_strerror(err));
        return 1;
    }

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    int capacity = MICROPHONE_LATENCY * 2 * in_stream->sample_rate() * in_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    auto ring_buffer = AudioRingBuffer::create(*audio, capacity);
    if (!ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }
    soundio_helper::ring_buffer = ring_buffer->ptr();

    if (err = in_stream->start()) {
        printf("unable to start input device: %s\n", soundio_strerror(err));
        return 1;
    }

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

    printf("Waiting for a Kinect Audio Receiver...\n");

    std::vector<uint8_t> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);
    if (error) {
        printf("Error receiving ping: %s\n", error.message().c_str());
        return 1;
    }

    SenderSocket sender(std::move(socket), remote_endpoint, 1024 * 1024);

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    OpusEncoder* opus_encoder = opus_encoder_create(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT, OPUS_APPLICATION_VOIP, &err);
    if (err < 0) {
        printf("failed to create an encoder: %s\n", opus_strerror(err));
        return 1;
    }

    int frame_id = 0;
    std::vector<uint8_t> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
    int sent_byte_count = 0;
    auto summary_time = std::chrono::steady_clock::now();
    for (;;) {
        audio->flushEvents();
        char* read_ptr = ring_buffer->getReadPtr();
        int fill_bytes = ring_buffer->getFillCount();

        const int FRAME_BYTE_SIZE = sizeof(float) * AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT;
        int cursor = 0;
        while ((fill_bytes - cursor) > FRAME_BYTE_SIZE) {
            unsigned char pcm_bytes[FRAME_BYTE_SIZE];
            memcpy(pcm_bytes, read_ptr + cursor, FRAME_BYTE_SIZE);

            int opus_frame_size = opus_encode_float(opus_encoder, reinterpret_cast<float*>(pcm_bytes), AUDIO_FRAME_SIZE, opus_frame.data(), KH_PACKET_SIZE);
            if (opus_frame_size < 0) {
                printf("encode failed: %s\n", opus_strerror(opus_frame_size));
                return 1;
            }

            sender.sendAudioPacket(0, frame_id++, opus_frame, opus_frame_size, error);

            cursor += FRAME_BYTE_SIZE;
            sent_byte_count += opus_frame_size;
        }

        ring_buffer->advanceReadPtr(cursor);
        
        auto summary_diff = std::chrono::steady_clock::now() - summary_time;
        if (summary_diff > std::chrono::seconds(5))
        {
            printf("Bandwidth: %f Mbps\n", (sent_byte_count / (1024.0f * 1024.0f / 8.0f)) / (summary_diff.count() / 1000000000.0f));
            sent_byte_count = 0;
            summary_time = std::chrono::steady_clock::now();
        }
    }
    opus_encoder_destroy(opus_encoder);
    return 0;
}
}

int main()
{
    return kh::main(7777);
}