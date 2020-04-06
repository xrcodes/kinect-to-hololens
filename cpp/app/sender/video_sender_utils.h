#pragma once

#include <iostream>
#include <vector>
#include "native/kh_native.h"

namespace kh
{
using Bytes = std::vector<std::byte>;
constexpr static int XOR_MAX_GROUP_SIZE{2};

class RemoteReceiver
{
public:
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};

    const asio::ip::udp::endpoint endpoint;
    int video_frame_id;

    RemoteReceiver(asio::ip::udp::endpoint endpoint)
        : endpoint{endpoint}, video_frame_id{INITIAL_VIDEO_FRAME_ID}
    {
    }
};

struct ReceiverReportSummary
{
    TimePoint start_time{TimePoint::now()};
    float decoder_time_ms_sum{0.0f};
    float frame_interval_ms_sum{0.0f};
    int received_report_count{0};
};

// This class includes both video packet bytes and parity packet bytes
// for a video frame.
struct VideoParityPacketByteSet
{
    const TimePoint time_point;
    const int frame_id;
    std::vector<std::vector<std::byte>> video_packet_byte_set;
    std::vector<std::vector<std::byte>> parity_packet_byte_set;

    VideoParityPacketByteSet(int frame_id,
                             std::vector<std::vector<std::byte>>&& video_packet_byte_set,
                             std::vector<std::vector<std::byte>>&& parity_packet_byte_set)
        : time_point{TimePoint::now()}, frame_id{frame_id}, video_packet_byte_set{video_packet_byte_set}, parity_packet_byte_set{parity_packet_byte_set}
    {
    }
};

class VideoParityPacketStorage
{
public:
    VideoParityPacketStorage()
        : video_parity_packet_byte_sets_{}
    {
    }

    void add(int frame_id,
             std::vector<std::vector<std::byte>>&& video_packet_byte_set,
             std::vector<std::vector<std::byte>>&& parity_packet_byte_set)
    {
        video_parity_packet_byte_sets_.insert({frame_id, VideoParityPacketByteSet(frame_id,
                                                                               std::move(video_packet_byte_set),
                                                                               std::move(parity_packet_byte_set))});
    }

    bool has(int frame_id)
    {
        return video_parity_packet_byte_sets_.find(frame_id) != video_parity_packet_byte_sets_.end();
    }

    VideoParityPacketByteSet& get(int frame_id)
    {
        return video_parity_packet_byte_sets_.at(frame_id);
    }

    void cleanup(float timeout_sec)
    {
        // Remove video packets from its container when the receiver already received them.
        for (auto it = video_parity_packet_byte_sets_.begin(); it != video_parity_packet_byte_sets_.end();) {
            if (it->second.time_point.elapsed_time().sec() > timeout_sec) {
                it = video_parity_packet_byte_sets_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<int, VideoParityPacketByteSet> video_parity_packet_byte_sets_;
};

class VideoPacketRetransmitter
{
public:
    VideoPacketRetransmitter(const int session_id)
        : session_id_{session_id}
    {
    }

    void retransmit(UdpSocket& udp_socket,
                    std::vector<RequestReceiverPacketData>& request_packet_data_vector,
                    VideoParityPacketStorage& video_parity_packet_storage,
                    const asio::ip::udp::endpoint remote_endpoint)
    {
        // Resend the requested video packets.
        for (auto& request_receiver_packet_data : request_packet_data_vector) {
            const int frame_id{request_receiver_packet_data.frame_id};
            if (!video_parity_packet_storage.has(frame_id))
                continue;

            for (int packet_index : request_receiver_packet_data.video_packet_indices)
                udp_socket.send(video_parity_packet_storage.get(frame_id).video_packet_byte_set[packet_index], remote_endpoint);

            for (int packet_index : request_receiver_packet_data.parity_packet_indices)
                udp_socket.send(video_parity_packet_storage.get(frame_id).parity_packet_byte_set[packet_index], remote_endpoint);
        }
    }

private:
    const int session_id_;
};
}