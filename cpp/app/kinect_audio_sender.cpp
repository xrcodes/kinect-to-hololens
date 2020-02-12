#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include <opus/opus.h>
#include "helper/soundio_helper.h"
#include "kh_packet_helper.h"

namespace kh
{
int main(int port)
{
    const int AZURE_KINECT_SAMPLE_RATE = 48000;
    const double MICROPHONE_LATENCY = 0.2; // seconds
    const int FRAME_SIZE = 960;
    
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
    in_stream->set_format(SoundIoFormatS16LE);
    in_stream->set_sample_rate(AZURE_KINECT_SAMPLE_RATE);
    in_stream->set_layout(*soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0));
    in_stream->set_software_latency(MICROPHONE_LATENCY);
    in_stream->set_read_callback(libsoundio::helper::azure_kinect_read_callback);
    in_stream->set_overflow_callback(libsoundio::helper::overflow_callback);
    if (err = in_stream->open()) {
        printf("unable to open input stream: %s\n", soundio_strerror(err));
        return 1;
    }

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    const int STEREO_CHANNEL_COUNT = 2;
    int capacity = MICROPHONE_LATENCY * 2 * in_stream->sample_rate() * in_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    libsoundio::helper::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    if (!libsoundio::helper::ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }

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

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    OpusEncoder* opus_encoder = opus_encoder_create(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT, OPUS_APPLICATION_VOIP, &err);
    if (err < 0) {
        printf("failed to create an encoder: %s\n", opus_strerror(err));
        return 1;
    }

    OpusDecoder* opus_decoder = opus_decoder_create(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT, &err);
    if (err < 0) {
        printf("failed to create decoder: %s\n", opus_strerror(err));
        return 1;
    }

    const int MAX_FRAME_SIZE = 6 * 960;
    const int MAX_PACKET_SIZE = 3 * 1276;

    opus_int16 in[FRAME_SIZE * STEREO_CHANNEL_COUNT];
    opus_int16 out[MAX_FRAME_SIZE * STEREO_CHANNEL_COUNT];
    unsigned char cbits[MAX_PACKET_SIZE];

    int sent_byte_count = 0;
    auto summary_time = std::chrono::steady_clock::now();
    for (;;) {
        audio->flushEvents();
        char* read_ptr = soundio_ring_buffer_read_ptr(libsoundio::helper::ring_buffer);
        int fill_bytes = soundio_ring_buffer_fill_count(libsoundio::helper::ring_buffer);

        if (fill_bytes <= 0)
            continue;

        const int FRAME_BYTE_SIZE = sizeof(short) * FRAME_SIZE * STEREO_CHANNEL_COUNT;

        int left_bytes = fill_bytes;
        int cursor = 0;

        while (left_bytes > FRAME_BYTE_SIZE) {
            unsigned char pcm_bytes[FRAME_BYTE_SIZE];
            memcpy(pcm_bytes, read_ptr + cursor, FRAME_BYTE_SIZE);

            //for (int i = 0; i < (FRAME_SIZE * STEREO_CHANNEL_COUNT); ++i) {
            for (int i = 0; i < 1920; ++i) {
                if (i >= 1920) {
                    printf("i: %d\n", i);
                }
                in[i] = pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i];
            }

            int opus_byte_count = opus_encode(opus_encoder, in, FRAME_SIZE, cbits, MAX_PACKET_SIZE);
            if (opus_byte_count < 0) {
                printf("encode failed: %s\n", opus_strerror(opus_byte_count));
                return 1;
            }

            int frame_size = opus_decode(opus_decoder, cbits, opus_byte_count, out, MAX_FRAME_SIZE, 0);
            if (frame_size < 0) {
                printf("decoder failed: %s\n", opus_strerror(frame_size));
                return 1;
            }

            for (int i = 0; i < STEREO_CHANNEL_COUNT * frame_size; i++) {
                pcm_bytes[2 * i] = out[i] & 0xFF;
                pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
            }


            int packet_size = 2 * STEREO_CHANNEL_COUNT * frame_size;
            printf("packet_size: %d, opus_byte_count: %d\n", packet_size, opus_byte_count);
            //socket.send_to(asio::buffer(pcm_bytes, 2 * STEREO_CHANNEL_COUNT * frame_size), remote_endpoint, 0, error);

            cursor += FRAME_BYTE_SIZE;
            left_bytes -= FRAME_BYTE_SIZE;
        }

        soundio_ring_buffer_advance_read_ptr(libsoundio::helper::ring_buffer, cursor);

        sent_byte_count += fill_bytes;
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
    return kh::main(7777);
}