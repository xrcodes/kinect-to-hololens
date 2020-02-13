#include <algorithm>
#include <iostream>
#include <map>
#include <asio.hpp>
#include <opus/opus.h>
#include "helper/soundio_helper.h"
#include "kh_receiver.h"
#include "kh_packet_helper.h"

namespace kh
{
int main(std::string ip_address, int port)
{
    const int AZURE_KINECT_SAMPLE_RATE = 48000;
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
    out_stream->set_format(SoundIoFormatFloat32LE);
    out_stream->set_sample_rate(AZURE_KINECT_SAMPLE_RATE);
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
    int capacity = MICROPHONE_LATENCY * 2 * out_stream->sample_rate() * out_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT;
    //int capacity = MICROPHONE_LATENCY * 2 * out_stream->sample_rate() * out_stream->bytes_per_sample() * STEREO_CHANNEL_COUNT / 2;
    //soundio_helper::ring_buffer = soundio_ring_buffer_create(audio->ptr(), capacity);
    auto ring_buffer = AudioRingBuffer::create(*audio, capacity);
    if (!ring_buffer) {
        printf("unable to create ring buffer: out of memory\n");
    }
    soundio_helper::ring_buffer = ring_buffer->ptr();

    int actual_capacity = soundio_ring_buffer_capacity(ring_buffer->ptr());
    printf("actual_capacity: %d, capacity: %d\n", actual_capacity, capacity);

    if (err = out_stream->start()) {
        printf("unable to start output device: %s\n", soundio_strerror(err));
        return 1;
    }

    asio::io_context io_context;
    Receiver receiver(io_context, 1024 * 1024);
    receiver.ping(ip_address, port);

    printf("start for loop\n");
    OpusDecoder* opus_decoder = opus_decoder_create(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT, &err);
    if (err < 0) {
        printf("failed to create decoder: %s\n", opus_strerror(err));
        return 1;
    }

    float out[AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT];

    std::map<int, std::vector<uint8_t>> packets;
    int received_byte_count = 0;
    int last_frame_id = -1;
    auto summary_time = std::chrono::steady_clock::now();
    for (;;) {
        audio->flushEvents();

        std::error_code error;
        while (auto packet = receiver.receive(error)) {
            received_byte_count += packet->size();

            int frame_id = copy_from_packet_data<int>(packet->data() + 5);
            packets.insert({ frame_id, std::move(*packet) });
        }

        while (packets.size() > 20) {
            printf("remove packet");
            packets.erase(packets.begin());
        }

        char* write_ptr = ring_buffer->getWritePtr();
        int free_bytes = ring_buffer->getFreeCount();
        //printf("free_bytes: %d\n", free_bytes);
        //printf("latency: %f\n", ring_buffer->getFillCount() / static_cast<float>(out_stream->sample_rate() * out_stream->bytes_per_frame()));
        //printf("packet size: %ld\n", packets.size());

        const int FRAME_BYTE_SIZE = sizeof(float) * AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT;

        int write_cursor = 0;
        auto packet_it = packets.begin();
        while((free_bytes - write_cursor) > FRAME_BYTE_SIZE) {
            if (packet_it == packets.end())
                break;

            int audio_packet_cursor = 5;
            int frame_id = copy_from_packet<int>(packet_it->second, audio_packet_cursor);

            int frame_size;
            if (frame_id <= last_frame_id) {
                // If a packet is about the past, throw it away and try again.
                packet_it = packets.erase(packet_it);
                continue;
            } else if (frame_id == last_frame_id + 1) {
                // When the packet for the next audio frame is found,
                // use it and erase it.
                int opus_frame_size = copy_from_packet<int>(packet_it->second, audio_packet_cursor);
                frame_size = opus_decode_float(opus_decoder, packet_it->second.data() + audio_packet_cursor, opus_frame_size, out, AUDIO_FRAME_SIZE, 0);
                packet_it = packets.erase(packet_it);
            } else {
                // If not, let opus know there is a packet loss.
                frame_size = opus_decode_float(opus_decoder, nullptr, 0, out, AUDIO_FRAME_SIZE, 0);
            }

            if (frame_size < 0) {
                printf("decoder failed: %s\n", opus_strerror(frame_size));
                return 1;
            }

            //printf("frame_id: %d, opus_frame_size: %d, frame_size: %d\n", frame_id, opus_frame_size, frame_size);
            memcpy(write_ptr + write_cursor, out, FRAME_BYTE_SIZE);

            ++last_frame_id;
            write_cursor += FRAME_BYTE_SIZE;
        }

        ring_buffer->advanceWritePtr(write_cursor);

        auto summary_diff = std::chrono::steady_clock::now() - summary_time;
        if (summary_diff > std::chrono::seconds(5))
        {
            printf("Bandwidth: %f Mbps\n", (received_byte_count / (1024.0f * 1024.0f / 8.0f)) / (summary_diff.count() / 1000000000.0f));
            received_byte_count = 0;
            summary_time = std::chrono::steady_clock::now();
        }
    }
    opus_decoder_destroy(opus_decoder);
    return 0;
}
}

int main()
{
    return kh::main("127.0.0.1", 7777);
}