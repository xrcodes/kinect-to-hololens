#pragma once

#include "helper/soundio_helper.h"

namespace kh
{
class AudioPacketSender
{
public:
    AudioPacketSender(const int session_id, const asio::ip::udp::endpoint remote_endpoint)
        : session_id_{session_id}, remote_endpoint_{remote_endpoint}, audio_{}, kinect_microphone_stream_{create_kinect_microphone_stream(audio_)},
        audio_encoder_{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false}, pcm_{}, audio_frame_id_{0}
    {
        constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio_.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        kinect_microphone_stream_.start();
    }

    void send(UdpSocket& udp_socket)
    {
        soundio_flush_events(audio_.get());
        char* read_ptr{soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer)};
        int fill_bytes{soundio_ring_buffer_fill_count(soundio_callback::ring_buffer)};

        const int BYTES_PER_FRAME{gsl::narrow_cast<int>(sizeof(float) * pcm_.size())};
        int cursor = 0;
        while ((fill_bytes - cursor) > BYTES_PER_FRAME) {
            memcpy(pcm_.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder_.encode(opus_frame.data(), pcm_.data(), KH_SAMPLES_PER_FRAME, gsl::narrow_cast<opus_int32>(opus_frame.size()));
            opus_frame.resize(opus_frame_size);

            udp_socket.send(create_audio_sender_packet_bytes(session_id_, audio_frame_id_++, opus_frame), remote_endpoint_);

            cursor += BYTES_PER_FRAME;
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);
    }

private:
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;

    Audio audio_;
    AudioInStream kinect_microphone_stream_;
    AudioEncoder audio_encoder_;

    std::array<float, KH_SAMPLES_PER_FRAME* KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};
}