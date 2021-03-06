#pragma once

#include "win32/soundio_utils.h"

namespace kh
{
static SoundIoRingBuffer* ring_buffer{nullptr};

void write_callback(SoundIoOutStream* outstream, int frame_count_min, int frame_count_max)
{
    write_buffer_to_outstream(outstream, frame_count_min, frame_count_max, ring_buffer);
}

void underflow_callback(struct SoundIoOutStream* outstream) {
    static int count = 0;
    // This line leaves too many logs...
    //printf("underflow %d\n", ++count);
}

// AudioPacketPlayer better suits what this class does.
// However, this is named receiver to match the corresponding c# class,
// which only receives packets and enqueues them to a ring buffer.
class AudioReceiver
{
public:
    AudioReceiver()
        : sound_io_{create_sound_io_handle()}, default_speaker_stream_{create_default_speaker_stream(sound_io_, write_callback, underflow_callback)},
        opus_decoder_{tt::create_opus_decoder_handle(KH_SAMPLE_RATE, KH_CHANNEL_COUNT)},
        pcm_{}, last_audio_frame_id_{-1}
    {
        constexpr int capacity{gsl::narrow<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

        ring_buffer = soundio_ring_buffer_create(sound_io_.get(), capacity);
        if (!ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        if (int error = soundio_outstream_start(default_speaker_stream_.get()))
            throw std::runtime_error(std::string("Failed to start AudioOutStream: ") + std::to_string(error));
    }

    void receive(std::vector<tt::AudioSenderPacket>& audio_packets)
    {
        constexpr float AMPLIFIER{8.0f};

        soundio_flush_events(sound_io_.get());

        if (audio_packets.empty())
            return;

        std::sort(audio_packets.begin(),
                  audio_packets.end(),
                  [](tt::AudioSenderPacket& a, tt::AudioSenderPacket& b) { return a.frame_id < b.frame_id; });

        char* write_ptr{soundio_ring_buffer_write_ptr(ring_buffer)};
        int free_bytes{soundio_ring_buffer_free_count(ring_buffer)};

        const int FRAME_BYTE_SIZE{gsl::narrow<int>(sizeof(float) * pcm_.size())};

        int write_cursor{0};
        auto packet_it = audio_packets.begin();
        while ((free_bytes - write_cursor) >= FRAME_BYTE_SIZE) {
            if (packet_it == audio_packets.end())
                break;

            int frame_size;
            if (packet_it->frame_id <= last_audio_frame_id_) {
                // If a packet is about the past, throw it away and try again.
                ++packet_it;
                continue;
            }

            //frame_size = opus_decoder_.decode(packet_it->opus_frame, pcm_.data(), KH_SAMPLES_PER_FRAME, 0);
            frame_size = tt::decode_opus(opus_decoder_, packet_it->opus_frame, pcm_.data(), KH_SAMPLES_PER_FRAME, 0);

            if (frame_size < 0)
                throw std::runtime_error(std::string("Failed to decode audio: ") + opus_strerror(frame_size));

            for (gsl::index i{0}; i < pcm_.size(); ++i)
                pcm_[i] *= AMPLIFIER;

            memcpy(write_ptr + write_cursor, pcm_.data(), FRAME_BYTE_SIZE);

            last_audio_frame_id_ = packet_it->frame_id;
            ++packet_it;
            write_cursor += FRAME_BYTE_SIZE;
        }

        soundio_ring_buffer_advance_write_ptr(ring_buffer, write_cursor);
    }

private:
    SoundIoHandle sound_io_;
    SoundIoOutStreamHandle default_speaker_stream_;

    tt::OpusDecoderHandle opus_decoder_;
    std::array<float, KH_SAMPLES_PER_FRAME* KH_CHANNEL_COUNT> pcm_;

    int last_audio_frame_id_;
};
}