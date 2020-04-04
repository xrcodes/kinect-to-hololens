#pragma once

#include "video_sender_utils.h"

namespace kh
{
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
    VideoPacketSender(const int session_id, const asio::ip::udp::endpoint remote_endpoint)
        : session_id_{session_id}, remote_endpoint_{remote_endpoint}, video_fec_packet_byte_sets_{}, video_frame_send_times_{}
    {
    }

    void send(UdpSocket& udp_socket,
              moodycamel::ReaderWriterQueue<ReportReceiverPacketData>& report_packet_data_queue,
              moodycamel::ReaderWriterQueue<RequestReceiverPacketData>& request_packet_data_queue,
              moodycamel::ReaderWriterQueue<VideoFecPacketByteSet>& video_fec_packet_byte_set_queue,
              ReceiverState& receiver_state,
              VideoPacketSenderSummary& summary)
    {
        // Update receiver_state and summary with Report packets.
        ReportReceiverPacketData report_receiver_packet_data;
        while (report_packet_data_queue.try_dequeue(report_receiver_packet_data)) {
            // Ignore if network is somehow out of order and a report comes in out of order.
            if (report_receiver_packet_data.frame_id <= receiver_state.video_frame_id)
                continue;
            
            receiver_state.video_frame_id = report_receiver_packet_data.frame_id;

            const auto round_trip_time{TimePoint::now() - video_frame_send_times_[receiver_state.video_frame_id]};
            summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
            summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
            summary.round_trip_ms_sum += round_trip_time.ms();
            ++summary.received_report_count;
        }

        // Resend the requested video packets.
        RequestReceiverPacketData request_receiver_packet_data;
        while (request_packet_data_queue.try_dequeue(request_receiver_packet_data)) {
            for (int packet_index : request_receiver_packet_data.packet_indices) {
                if (video_fec_packet_byte_sets_.find(request_receiver_packet_data.frame_id) == video_fec_packet_byte_sets_.end())
                    continue;

                udp_socket.send(video_fec_packet_byte_sets_[request_receiver_packet_data.frame_id].video_packet_byte_set[packet_index], remote_endpoint_);
            }
        }

        // With the new video packets from Kinect, make FEC packets for them and send the both types packets.
        VideoFecPacketByteSet video_fec_packet_byte_set;
        while (video_fec_packet_byte_set_queue.try_dequeue(video_fec_packet_byte_set)) {
            video_frame_send_times_.insert({video_fec_packet_byte_set.frame_id, TimePoint::now()});
            for (auto& packet : video_fec_packet_byte_set.video_packet_byte_set) {
                std::error_code error;
                udp_socket.send(packet, remote_endpoint_);
            }

            for (auto& fec_packet_bytes : video_fec_packet_byte_set.fec_packet_byte_set) {
                std::error_code error;
                udp_socket.send(fec_packet_bytes, remote_endpoint_);
            }

            // Save the video packets to resend them when requested.
            video_fec_packet_byte_sets_.insert({video_fec_packet_byte_set.frame_id, std::move(video_fec_packet_byte_set)});
        }

        // Remove video packets from its container when the receiver already received them.
        for (auto it = video_fec_packet_byte_sets_.begin(); it != video_fec_packet_byte_sets_.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_fec_packet_byte_sets_.erase(it);
            } else {
                ++it;
            }
        }

        // No need to save its profiling information when the video frame is over.
        for (auto it = video_frame_send_times_.begin(); it != video_frame_send_times_.end();) {
            if (it->first <= receiver_state.video_frame_id) {
                it = video_frame_send_times_.erase(it);
            } else {
                ++it;
            }
        }
    }

public:
private:
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;

    std::unordered_map<int, VideoFecPacketByteSet> video_fec_packet_byte_sets_;
    std::unordered_map<int, TimePoint> video_frame_send_times_;
};
}