#pragma once

#include <vector>

namespace kh
{
using Bytes = std::vector<std::byte>;

struct ReceiverState
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};
    int video_frame_id{INITIAL_VIDEO_FRAME_ID};
};

struct VideoPacketSenderSummary
{
    TimePoint start_time{TimePoint::now()};
    float decoder_time_ms_sum{0.0f};
    float frame_interval_ms_sum{0.0f};
    float round_trip_ms_sum{0.0f};
    int received_report_count{0};
};

class VideoPacketSender
{
public:
    VideoPacketSender()
        : video_packet_sets_{}, video_frame_send_times_{}
    {

    }

    void send(const int session_id,
              UdpSocket& udp_socket,
              moodycamel::ReaderWriterQueue<std::pair<int, std::vector<Bytes>>>& video_packet_queue,
              ReceiverState& receiver_state,
              VideoPacketSenderSummary& summary)
    {
        while (auto received_packet{udp_socket.receive()}) {
            switch (get_packet_type_from_receiver_packet_bytes(*received_packet)) {
            case ReceiverPacketType::Report: {
                const auto report_receiver_packet_data{parse_report_receiver_packet_bytes(*received_packet)};
                receiver_state.video_frame_id = report_receiver_packet_data.frame_id;

                const auto round_trip_time{TimePoint::now() - video_frame_send_times_[receiver_state.video_frame_id]};
                summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
                summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
                summary.round_trip_ms_sum += round_trip_time.ms();
                ++summary.received_report_count;
            }
            break;
            case ReceiverPacketType::Request: {
                const auto request_receiver_packet_data{parse_request_receiver_packet_bytes(*received_packet)};

                for (int packet_index : request_receiver_packet_data.packet_indices) {
                    if (video_packet_sets_.find(request_receiver_packet_data.frame_id) == video_packet_sets_.end())
                        continue;

                    udp_socket.send(video_packet_sets_[request_receiver_packet_data.frame_id].second[packet_index]);
                }
            }
            break;
            }
        }

        std::pair<int, std::vector<Bytes>> video_packet_set;
        while (video_packet_queue.try_dequeue(video_packet_set)) {
            auto fec_packet_bytes_set{create_fec_sender_packet_bytes_set(session_id, video_packet_set.first, XOR_MAX_GROUP_SIZE, video_packet_set.second)};

            video_frame_send_times_.insert({video_packet_set.first, TimePoint::now()});
            for (auto& packet : video_packet_set.second) {
                std::error_code error;
                udp_socket.send(packet);
            }

            for (auto& fec_packet_bytes : fec_packet_bytes_set) {
                std::error_code error;
                udp_socket.send(fec_packet_bytes);
            }
            video_packet_sets_.insert({video_packet_set.first, std::move(video_packet_set)});
        }

        // Remove elements of frame_packet_sets reserved for filling up missing packets
        // if they are already used from the receiver side.
        for (auto it = video_packet_sets_.begin(); it != video_packet_sets_.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_packet_sets_.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = video_frame_send_times_.begin(); it != video_frame_send_times_.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_frame_send_times_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    constexpr static int XOR_MAX_GROUP_SIZE{5};
    std::unordered_map<int, std::pair<int, std::vector<Bytes>>> video_packet_sets_;
    std::unordered_map<int, TimePoint> video_frame_send_times_;
    VideoPacketSenderSummary summary;
};

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

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm_;
    int audio_frame_id_;
};
}