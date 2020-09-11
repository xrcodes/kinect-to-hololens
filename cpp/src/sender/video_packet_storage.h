#pragma once

#include "native/kh_native.h"

namespace kh
{
struct VideoFramePackets
{
    const int frame_id;
    const tt::TimePoint creation_time_point;
    std::vector<Packet> video_packets;
    std::vector<Packet> parity_packets;

    VideoFramePackets(int frame_id, std::vector<Packet>&& video_packets, std::vector<Packet>&& parity_packets)
        : frame_id{frame_id}, creation_time_point{tt::TimePoint::now()}, video_packets{video_packets}, parity_packets{parity_packets}
    {
    }
};

class VideoPacketStorage
{
public:
    VideoPacketStorage()
        : video_frame_packets_{}
    {
    }

    void add(int frame_id, std::vector<Packet>&& video_packets, std::vector<Packet>&& parity_packets)
    {
        video_frame_packets_.insert({frame_id, VideoFramePackets(frame_id, std::move(video_packets), std::move(parity_packets))});
    }

    auto find(int frame_id)
    {
        return video_frame_packets_.find(frame_id);
    }

    auto end()
    {
        return video_frame_packets_.end();
    }

    // For removing packets from their container after some time.
    void cleanup(float timeout_sec)
    {
        for (auto it{video_frame_packets_.begin()}; it != video_frame_packets_.end();) {
            if (it->second.creation_time_point.elapsed_time().sec() > timeout_sec) {
                it = video_frame_packets_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    std::unordered_map<int, VideoFramePackets> video_frame_packets_;
};
}