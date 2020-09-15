#pragma once

#include "sender/remote_receiver.h"
#include "native/kh_native.h"

namespace kh
{
SoundIoRingBuffer* ring_buffer{nullptr};

void read_callback(SoundIoInStream* instream, int frame_count_min, int frame_count_max)
{
    // Using KH_CHANNEL_COUNT (i.e., 2) to pick the first two channels of the Azure Kinect microphone.
    // Use (instream->bytes_per_frame / instream->bytes_per_sample) as channel_count for full usage of channels.
    write_instream_to_buffer(instream, frame_count_min, frame_count_max, ring_buffer, KH_CHANNEL_COUNT);
}

void overflow_callback(struct SoundIoInStream* instream) {
    static int count = 0;
    printf("overflow %d\n", ++count);
}

// The instance of this class uses soundio to access Kinect's microphone.
// Code below looks ugly because soundio requires its callback functions to be
// c-style functions, not a member function, and this class is trying to
// cover the discrepancy inside here.
class AudioSender
{
public:
    AudioSender(const int sender_id)
        : sender_id_{sender_id}
        , sound_io_{create_sound_io_handle()}
        , kinect_microphone_stream_{create_kinect_microphone_stream(sound_io_, read_callback, overflow_callback)}
        , opus_encoder_{tt::create_opus_encoder_handle(KH_SAMPLE_RATE, KH_CHANNEL_COUNT)}
        , pcm_{}
        , audio_frame_id_{0}
    {
        constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};
        ring_buffer = soundio_ring_buffer_create(sound_io_.get(), capacity);
        if (!ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        if (int error = soundio_instream_start(kinect_microphone_stream_.get()))
            throw std::runtime_error(std::string("Failed to start AudioInStream: ") + std::to_string(error));
    }

    void send(UdpSocket& udp_socket, std::unordered_map<int, RemoteReceiver>& remote_receivers)
    {
        soundio_flush_events(sound_io_.get());
        const char* read_ptr{soundio_ring_buffer_read_ptr(ring_buffer)};
        const int fill_bytes{soundio_ring_buffer_fill_count(ring_buffer)};

        constexpr int BYTES_PER_FRAME{gsl::narrow<int>(sizeof(float) * KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT)};
        int cursor = 0;
        while ((fill_bytes - cursor) >= BYTES_PER_FRAME) {
            memcpy(pcm_.data(), read_ptr + cursor, BYTES_PER_FRAME);

            std::vector<std::byte> opus_frame(KH_MAX_AUDIO_PACKET_CONTENT_SIZE);
            const int opus_frame_size{tt::encode_opus(opus_encoder_,
                                                      opus_frame.data(),
                                                      pcm_.data(),
                                                      KH_SAMPLES_PER_FRAME,
                                                      gsl::narrow<opus_int32>(opus_frame.size()))};
            opus_frame.resize(opus_frame_size);
            for (auto& [_, remote_receiver] : remote_receivers) {
                if (remote_receiver.audio_requested) {
                    udp_socket.send(create_audio_sender_packet(sender_id_, audio_frame_id_++, opus_frame).bytes, remote_receiver.endpoint);
                }
            }
            cursor += BYTES_PER_FRAME;
        }

        soundio_ring_buffer_advance_read_ptr(ring_buffer, cursor);
    }

private:
    const int sender_id_;

    SoundIoHandle sound_io_;
    SoundIoInStreamHandle kinect_microphone_stream_;
    tt::OpusEncoderHandle opus_encoder_;

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};
}