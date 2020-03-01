#include <algorithm>
#include <iostream>
#include <asio.hpp>
#include "kh_opus.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "helper/soundio_callback.h"

namespace kh
{
int main(int port)
{
    constexpr int SENDER_SEND_BUFFER_SIZE = 1024 * 1024;

    Audio audio;
    auto kinect_microphone{find_kinect_microphone(audio)};
    AudioInStream kinect_microphone_stream{kinect_microphone};
    // These settings came from tools/k4aviewer/k4amicrophone.cpp of Azure-Kinect-Sensor-SDK.
    kinect_microphone_stream.get()->format = SoundIoFormatFloat32LE;
    kinect_microphone_stream.get()->sample_rate = KH_SAMPLE_RATE;
    kinect_microphone_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutId7Point0);
    kinect_microphone_stream.get()->software_latency = KH_LATENCY_SECONDS;
    kinect_microphone_stream.get()->read_callback = soundio_callback::kinect_microphone_read_callback;
    kinect_microphone_stream.get()->overflow_callback = soundio_callback::overflow_callback;
    kinect_microphone_stream.open();

    // While the Azure Kinect is set to have 7.0 channel layout, which has 7 channels, only two of them gets used.
    // Therefore, we use bytes_per_sample * 2 instead of bytes_per_frame.
    const int kinect_microphone_bytes_per_second{kinect_microphone_stream.get()->sample_rate * kinect_microphone_stream.get()->bytes_per_sample * KH_CHANNEL_COUNT};
    assert(KH_BYTES_PER_SECOND == kinect_microphone_bytes_per_second);

    constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

    soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    if (!soundio_callback::ring_buffer)
        throw std::exception("Failed in soundio_ring_buffer_create()...");

    asio::io_context io_context;
    asio::ip::udp::socket socket{io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)};
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});

    std::cout << "Waiting for a Kinect Audio Receiver...\n";

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
    AudioEncoder audio_encoder{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false};

    kinect_microphone_stream.start();

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm;

    int frame_id{0};
    int sent_byte_count{0};
    auto summary_time{TimePoint::now()};
    for (;;) {
        soundio_flush_events(audio.get());
        char* read_ptr = soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer);
        int fill_bytes = soundio_ring_buffer_fill_count(soundio_callback::ring_buffer);

        constexpr int BYTES_PER_FRAME{sizeof(float) * pcm.size()};
        int cursor = 0;
        while ((fill_bytes - cursor) > BYTES_PER_FRAME) {
            memcpy(pcm.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder.encode(opus_frame.data(), pcm.data(), KH_SAMPLES_PER_FRAME, opus_frame.size());
            opus_frame.resize(opus_frame_size);

            udp_socket.send(create_audio_sender_packet_bytes(0, frame_id++, opus_frame), asio_error);
            if (asio_error)
                throw std::runtime_error(std::string("Error sending audio packets: ") + asio_error.message());

            cursor += BYTES_PER_FRAME;
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