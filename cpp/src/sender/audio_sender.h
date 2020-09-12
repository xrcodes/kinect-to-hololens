#pragma once

#include "sender/remote_receiver.h"
#include "utils/soundio_utils.h"

namespace kh
{
// The instance of this class uses soundio to access Kinect's microphone.
// Code below looks ugly because soundio requires its callback functions to be
// c-style functions, not a member function, and this class is trying to
// cover the discrepancy inside here.
class AudioSender
{
public:
    AudioSender(const int sender_id)
        : sender_id_{sender_id}
        , audio_{}
        , kinect_microphone_stream_{create_kinect_microphone_stream(audio_)}
        , audio_encoder_{KH_SAMPLE_RATE, KH_CHANNEL_COUNT, false}
        , pcm_{}
        , audio_frame_id_{0}
    {
        constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio_.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        kinect_microphone_stream_.start();
    }

    void send(UdpSocket& udp_socket, std::unordered_map<int, RemoteReceiver>& remote_receivers)
    {
        soundio_flush_events(audio_.get());
        const char* read_ptr{soundio_ring_buffer_read_ptr(soundio_callback::ring_buffer)};
        const int fill_bytes{soundio_ring_buffer_fill_count(soundio_callback::ring_buffer)};

        constexpr int BYTES_PER_FRAME{gsl::narrow<int>(sizeof(float) * KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT)};
        int cursor = 0;
        while ((fill_bytes - cursor) >= BYTES_PER_FRAME) {
            memcpy(pcm_.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            const int opus_frame_size{audio_encoder_.encode(opus_frame.data(),
                                                            pcm_.data(),
                                                            KH_SAMPLES_PER_FRAME,
                                                            gsl::narrow<opus_int32>(opus_frame.size()))};
            opus_frame.resize(opus_frame_size);
            for (auto& [_, remote_receiver] : remote_receivers) {
                if(remote_receiver.audio_requested)
                    udp_socket.send(create_audio_sender_packet(sender_id_, audio_frame_id_++, opus_frame).bytes, remote_receiver.endpoint);
            }
            cursor += BYTES_PER_FRAME;
        }

        soundio_ring_buffer_advance_read_ptr(soundio_callback::ring_buffer, cursor);
    }

private:
    const int sender_id_;

    Audio audio_;
    AudioInStream kinect_microphone_stream_;
    tt::AudioEncoder audio_encoder_;

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};
}