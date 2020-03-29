#pragma once

namespace kh
{
class AudioPacketSender
{
public:
    AudioPacketSender()
        : audio_{}, kinect_microphone_stream_{create_kinect_microphone_stream(audio_)},
        audio_encoder_{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false}, pcm_{}, audio_frame_id_{0}
    {
        constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio_.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        kinect_microphone_stream_.start();
    }

    void send(int session_id, UdpSocket& udp_socket)
    {
        soundio_flush_events(audio_.get());
        char* read_ptr{soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer)};
        int fill_bytes{soundio_ring_buffer_fill_count(soundio_callback::ring_buffer)};

        const int BYTES_PER_FRAME{gsl::narrow_cast<int>(sizeof(float) * pcm_.size())};
        int cursor = 0;
        while ((fill_bytes - cursor) > BYTES_PER_FRAME) {
            memcpy(pcm_.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            int opus_frame_size = audio_encoder_.encode(opus_frame.data(), pcm_.data(), KH_SAMPLES_PER_FRAME, opus_frame.size());
            opus_frame.resize(opus_frame_size);

            std::error_code error;
            udp_socket.send(create_audio_sender_packet_bytes(session_id, audio_frame_id_++, opus_frame));

            cursor += BYTES_PER_FRAME;
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);
    }

private:
    Audio audio_;
    AudioInStream kinect_microphone_stream_;
    AudioEncoder audio_encoder_;

    std::array<float, KH_SAMPLES_PER_FRAME* KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};
}