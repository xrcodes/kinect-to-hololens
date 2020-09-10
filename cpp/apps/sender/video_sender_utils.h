#pragma once

#include "native/kh_native.h"

namespace kh
{
using Bytes = std::vector<std::byte>;

// This class includes both video packet bytes and parity packet bytes
// for a video frame.
struct VideoPacketByteSet
{
    const int frame_id;
    const tt::TimePoint creation_time_point;
    std::vector<Bytes> video_packet_byte_set;
    std::vector<Bytes> parity_packet_byte_set;

    VideoPacketByteSet(int frame_id,
                       std::vector<std::vector<std::byte>>&& video_packet_byte_set,
                       std::vector<std::vector<std::byte>>&& parity_packet_byte_set)
        : frame_id{frame_id}, creation_time_point{tt::TimePoint::now()}, video_packet_byte_set{video_packet_byte_set}, parity_packet_byte_set{parity_packet_byte_set}
    {
    }
};

class VideoPacketStorage
{
public:
    VideoPacketStorage()
        : video_packet_byte_sets_{}
    {
    }

    void add(int frame_id,
             std::vector<std::vector<std::byte>>&& video_packet_byte_set,
             std::vector<std::vector<std::byte>>&& parity_packet_byte_set)
    {
        video_packet_byte_sets_.insert({frame_id, VideoPacketByteSet(frame_id,
                                                                     std::move(video_packet_byte_set),
                                                                     std::move(parity_packet_byte_set))});
    }

    bool has(int frame_id)
    {
        return video_packet_byte_sets_.find(frame_id) != video_packet_byte_sets_.end();
    }

    VideoPacketByteSet& get(int frame_id)
    {
        return video_packet_byte_sets_.at(frame_id);
    }

    void cleanup(float timeout_sec)
    {
        // Remove video packets from its container when the receiver already received them.
        for (auto it{video_packet_byte_sets_.begin()}; it != video_packet_byte_sets_.end();) {
            if (it->second.creation_time_point.elapsed_time().sec() > timeout_sec) {
                it = video_packet_byte_sets_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<int, VideoPacketByteSet> video_packet_byte_sets_;
};
}