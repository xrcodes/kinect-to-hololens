#pragma once

#include <iostream>
#include <vector>
#include "native/kh_native.h"
#include "remote_receiver.h"

namespace kh
{
using Bytes = std::vector<std::byte>;

// This class includes both video packet bytes and parity packet bytes
// for a video frame.
struct VideoParityPacketByteSet
{
    const tt::TimePoint time_point;
    const int frame_id;
    std::vector<std::vector<std::byte>> video_packet_byte_set;
    std::vector<std::vector<std::byte>> parity_packet_byte_set;

    VideoParityPacketByteSet(int frame_id,
                             std::vector<std::vector<std::byte>>&& video_packet_byte_set,
                             std::vector<std::vector<std::byte>>&& parity_packet_byte_set)
        : time_point{tt::TimePoint::now()}, frame_id{frame_id}, video_packet_byte_set{video_packet_byte_set}, parity_packet_byte_set{parity_packet_byte_set}
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
        for (auto it{video_parity_packet_byte_sets_.begin()}; it != video_parity_packet_byte_sets_.end();) {
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
}