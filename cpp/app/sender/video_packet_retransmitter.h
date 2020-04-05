#pragma once

#include "video_sender_utils.h"

namespace kh
{
struct ReceiverReportSummary
{
    TimePoint start_time{TimePoint::now()};
    float decoder_time_ms_sum{0.0f};
    float frame_interval_ms_sum{0.0f};
    int received_report_count{0};
};

class VideoPacketRetransmitter
{
public:
    VideoPacketRetransmitter(const int session_id, const asio::ip::udp::endpoint remote_endpoint)
        : session_id_{session_id}, remote_endpoint_{remote_endpoint}
    {
    }

    void retransmit(UdpSocket& udp_socket,
              std::vector<RequestReceiverPacketData>& request_packet_data_vector,
              VideoFecPacketStorage& video_fec_packet_storage)
    {
        // Resend the requested video packets.
        for (auto& request_receiver_packet_data : request_packet_data_vector) {
            for (int packet_index : request_receiver_packet_data.packet_indices) {
                if (!video_fec_packet_storage.has(request_receiver_packet_data.frame_id))
                    continue;

                udp_socket.send(video_fec_packet_storage.get(request_receiver_packet_data.frame_id).video_packet_byte_set[packet_index], remote_endpoint_);
            }
        }
    }

private:
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;
};
}