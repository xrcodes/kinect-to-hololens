#pragma once

#include <vector>
#include "native/kh_native.h"

namespace kh
{
using Bytes = std::vector<std::byte>;
constexpr static int XOR_MAX_GROUP_SIZE{2};

struct ReceiverState
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};

    const asio::ip::udp::endpoint endpoint;
    int video_frame_id;

    ReceiverState(asio::ip::udp::endpoint endpoint)
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

// This class includes both video packet bytes and fec packet bytes
// for a video frame.
struct VideoFecPacketByteSet
{
    int frame_id;
    std::vector<std::vector<std::byte>> video_packet_byte_set;
    std::vector<std::vector<std::byte>> fec_packet_byte_set;

    VideoFecPacketByteSet(int frame_id,
                          std::vector<std::vector<std::byte>>&& video_packet_byte_set,
                          std::vector<std::vector<std::byte>>&& fec_packet_byte_set)
        : frame_id{frame_id}, video_packet_byte_set{video_packet_byte_set}, fec_packet_byte_set{fec_packet_byte_set}
    {
    }
};

class VideoFecPacketStorage
{
public:
    VideoFecPacketStorage()
        : video_fec_packet_byte_sets_{}
    {
    }

    void add(int frame_id,
             std::vector<std::vector<std::byte>>&& video_packet_byte_set,
             std::vector<std::vector<std::byte>>&& fec_packet_byte_set)
    {
        video_fec_packet_byte_sets_.insert({frame_id,
                                            VideoFecPacketByteSet(frame_id,
                                                                  std::move(video_packet_byte_set),
                                                                  std::move(fec_packet_byte_set))});
    }

    bool has(int frame_id)
    {
        return video_fec_packet_byte_sets_.find(frame_id) != video_fec_packet_byte_sets_.end();
    }

    VideoFecPacketByteSet& get(int frame_id)
    {
        return video_fec_packet_byte_sets_.at(frame_id);
    }

    void cleanup(int receiver_frame_id)
    {
        // Remove video packets from its container when the receiver already received them.
        for (auto it = video_fec_packet_byte_sets_.begin(); it != video_fec_packet_byte_sets_.end();) {
            if (it->first <= receiver_frame_id) {
                it = video_fec_packet_byte_sets_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<int, VideoFecPacketByteSet> video_fec_packet_byte_sets_;
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