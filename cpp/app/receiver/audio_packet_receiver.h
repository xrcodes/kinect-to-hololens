#pragma once

namespace kh
{
// AudioPacketPlayer better suits what this class does.
// However, this is named receiver to match the corresponding c# class,
// which only receives packets and enqueues them to a ring buffer.
class AudioPacketReceiver
{
public:
    AudioPacketReceiver()
        : audio_{}, default_speaker_stream_{create_default_speaker_stream(audio_)},
        audio_decoder_{KH_SAMPLE_RATE, KH_CHANNEL_COUNT},
        pcm_{}, last_audio_frame_id_{-1}
    {

        constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

        soundio_callback::ring_buffer = soundio_ring_buffer_create(audio_.get(), capacity);
        if (!soundio_callback::ring_buffer)
            throw std::runtime_error("Failed in soundio_ring_buffer_create()...");

        default_speaker_stream_.start();
    }

    void receive(moodycamel::ReaderWriterQueue<AudioSenderPacketData>& audio_packet_data_queue)
    {
        constexpr float AMPLIFIER{8.0f};

        soundio_flush_events(audio_.get());

        std::vector<AudioSenderPacketData> audio_packet_data_set;
        AudioSenderPacketData audio_sender_packet_data;
        while (audio_packet_data_queue.try_dequeue(audio_sender_packet_data))
            audio_packet_data_set.push_back(audio_sender_packet_data);

        if (audio_packet_data_set.empty())
            return;

        std::sort(audio_packet_data_set.begin(),
                  audio_packet_data_set.end(),
                  [](AudioSenderPacketData& a, AudioSenderPacketData& b) { return a.frame_id < b.frame_id; });

        char* write_ptr{soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer)};
        int free_bytes{soundio_ring_buffer_free_count(soundio_callback::ring_buffer)};

        const int FRAME_BYTE_SIZE{gsl::narrow_cast<int>(sizeof(float) * pcm_.size())};

        int write_cursor{0};
        auto packet_it = audio_packet_data_set.begin();
        while ((free_bytes - write_cursor) >= FRAME_BYTE_SIZE) {
            if (packet_it == audio_packet_data_set.end())
                break;

            int frame_size;
            if (packet_it->frame_id <= last_audio_frame_id_) {
                // If a packet is about the past, throw it away and try again.
                ++packet_it;
                continue;
            }

            frame_size = audio_decoder_.decode(packet_it->opus_frame, pcm_.data(), KH_SAMPLES_PER_FRAME, 0);

            if (frame_size < 0)
                throw std::runtime_error(std::string("Failed to decode audio: ") + opus_strerror(frame_size));

            for (gsl::index i{0}; i < pcm_.size(); ++i)
                pcm_[i] *= AMPLIFIER;

            memcpy(write_ptr + write_cursor, pcm_.data(), FRAME_BYTE_SIZE);

            last_audio_frame_id_ = packet_it->frame_id;
            ++packet_it;
            write_cursor += FRAME_BYTE_SIZE;
        }

        soundio_ring_buffer_advance_write_ptr(soundio_callback::ring_buffer, write_cursor);
    }

private:
    Audio audio_;
    AudioOutStream default_speaker_stream_;

    AudioDecoder audio_decoder_;
    std::array<float, KH_SAMPLES_PER_FRAME* KH_CHANNEL_COUNT> pcm_;

    int last_audio_frame_id_;
};
}