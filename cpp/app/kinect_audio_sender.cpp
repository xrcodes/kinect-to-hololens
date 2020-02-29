#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include <opus/opus.h>
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "helper/soundio_callback.h"

namespace kh
{
int main(int port)
{
    constexpr int KINECT_MICROPHONE_SAMPLE_RATE{48000};
    constexpr int STEREO_CHANNEL_COUNT{2};
    constexpr double MICROPHONE_LATENCY{0.2}; // seconds
    constexpr int AUDIO_FRAME_SIZE{960};
    
    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    AudioInStream kinect_microphone_stream{kinect_microphone};
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream.get()->format = SoundIoFormatFloat32LE;
    kinect_microphone_stream.get()->sample_rate = KINECT_MICROPHONE_SAMPLE_RATE;
    kinect_microphone_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    kinect_microphone_stream.get()->software_latency = MICROPHONE_LATENCY;
    kinect_microphone_stream.get()->read_callback = soundio_callback::azure_kinect_read_callback;
    kinect_microphone_stream.get()->overflow_callback = soundio_callback::overflow_callback;
    kinect_microphone_stream.open();

    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    // Therefore, we use bytes_per_sample * 2 instead of bytes_per_frame.
    const int bytes_per_second{kinect_microphone_stream.get()->sample_rate * kinect_microphone_stream.get()->bytes_per_sample * STEREO_CHANNEL_COUNT};
    const int capacity{gsl::narrow_cast<int>(MICROPHONE_LATENCY * 2 * bytes_per_second)};

    soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    if (!soundio_callback::ring_buffer) {
        printf("unable to create ring buffer\n");
        return 1;
    }

    asio::io_context io_context;
    asio::ip::udp::socket socket{io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)};
    socket.set_option(asio::socket_base::send_buffer_size{1024 * 1024});

    printf("Waiting for a Kinect Audio Receiver...\n");

    std::vector<uint8_t> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code asio_error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, asio_error);
    if (asio_error) {
        printf("Error receiving ping: %s\n", asio_error.message().c_str());
        return 1;
    }

    UdpSocket udp_socket{std::move(socket), remote_endpoint};
    // Consider using FEC in the future. Currently, Opus is good enough even without FEC.
    AudioEncoder audio_encoder{KINECT_MICROPHONE_SAMPLE_RATE, STEREO_CHANNEL_COUNT, false};

    kinect_microphone_stream.start();

    int frame_id{0};
    int sent_byte_count{0};
    auto summary_time{TimePoint::now()};
    for (;;) {
        soundio_flush_events(audio.get());
        char* read_ptr = soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer);
        int fill_bytes = soundio_ring_buffer_fill_count(soundio_callback::ring_buffer);

        constexpr int FRAME_BYTE_SIZE = sizeof(float) * AUDIO_FRAME_SIZE * STEREO_CHANNEL_COUNT;
        int cursor = 0;
        while ((fill_bytes - cursor) > FRAME_BYTE_SIZE) {
            unsigned char pcm_bytes[FRAME_BYTE_SIZE];
            memcpy(pcm_bytes, read_ptr + cursor, FRAME_BYTE_SIZE);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder.encode(opus_frame.data(), reinterpret_cast<float*>(pcm_bytes), AUDIO_FRAME_SIZE, KH_PACKET_SIZE);
            opus_frame.resize(opus_frame_size);

            udp_socket.send(create_audio_sender_packet_bytes(0, frame_id++, opus_frame), asio_error);
            if (asio_error) {
                printf("Error sending packets: %s\n", asio_error.message().c_str());
                return 1;
            }

            cursor += FRAME_BYTE_SIZE;
            sent_byte_count += opus_frame.size();
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);
        
        auto summary_diff = TimePoint::now() - summary_time;
        if (summary_diff.sec() > 5)
        {
            printf("Bandwidth: %f Mbps\n", (sent_byte_count / (1024.0f * 1024.0f / 8.0f)) / summary_diff.sec());
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