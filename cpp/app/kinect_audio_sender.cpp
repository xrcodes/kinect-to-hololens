#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include <opus/opus.h>
#include "helper/soundio_helper.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"

namespace kh
{
int main(int port)
{
    constexpr int AZURE_KINECT_SAMPLE_RATE{48000};
    constexpr int STEREO_CHANNEL_COUNT{2};
    constexpr double MICROPHONE_LATENCY{0.2}; // seconds
    constexpr int AUDIO_FRAME_SIZE{960};
    
    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    AudioInStream in_stream{kinect_microphone};
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    in_stream.get()->format = SoundIoFormatFloat32LE;
    in_stream.get()->sample_rate = AZURE_KINECT_SAMPLE_RATE;
    in_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    in_stream.get()->software_latency = MICROPHONE_LATENCY;
    in_stream.get()->read_callback = soundio_helper::azure_kinect_read_callback;
    in_stream.get()->overflow_callback = soundio_helper::overflow_callback;

    in_stream.open();

    //int capacity = microphone_latency * 2 * in_stream->ptr()->sample_rate * in_stream->ptr()->bytes_per_frame;
    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    const int capacity{gsl::narrow_cast<int>(MICROPHONE_LATENCY * 2 * in_stream.get()->sample_rate * in_stream.get()->bytes_per_sample * STEREO_CHANNEL_COUNT)};
    AudioRingBuffer ring_buffer{audio, capacity};
    soundio_helper::ring_buffer = ring_buffer.get();

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

    in_stream.start();

    UdpSocket udp_socket{std::move(socket), remote_endpoint};
    int error;
    OpusEncoder* opus_encoder{opus_encoder_create(AZURE_KINECT_SAMPLE_RATE, STEREO_CHANNEL_COUNT, OPUS_APPLICATION_VOIP, &error)};
    if (error < 0) {
        printf("failed to create an encoder: %s\n", opus_strerror(error));
        return 1;
    }

    int frame_id{0};
    std::vector<uint8_t> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
    int sent_byte_count{0};
    auto summary_time{TimePoint::now()};
    for (;;) {
        soundio_flush_events(audio.get());
        char* read_ptr = ring_buffer.getReadPtr();
        int fill_bytes = ring_buffer.getFillCount();

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

            const auto audio_sender_packet_bytes{create_audio_sender_packet_bytes(0, frame_id++,
                                                                                  gsl::span<std::byte>(reinterpret_cast<std::byte*>(opus_frame.data()), opus_frame_size))};
            udp_socket.send(audio_sender_packet_bytes, asio_error);
            if (asio_error) {
                printf("Error sending packets: %s\n", asio_error.message().c_str());
                return 1;
            }

            cursor += FRAME_BYTE_SIZE;
            sent_byte_count += opus_frame_size;
        }

        ring_buffer.advanceReadPtr(cursor);
        
        auto summary_diff = TimePoint::now() - summary_time;
        if (summary_diff.sec() > 5)
        {
            printf("Bandwidth: %f Mbps\n", (sent_byte_count / (1024.0f * 1024.0f / 8.0f)) / summary_diff.sec());
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