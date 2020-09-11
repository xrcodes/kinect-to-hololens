#pragma once

#include "native/kh_native.h"

namespace kh
{
struct VideoFramePackets
{
    std::vector<Packet> video_packets;
    std::vector<Packet> parity_packets;

    VideoFramePackets(std::vector<Packet>&& video_packets, std::vector<Packet>&& parity_packets)
        : video_packets{video_packets}, parity_packets{parity_packets}
    {
    }
};

class VideoSenderStorage
{
public:
    VideoSenderStorage()
        : video_frame_packets_{}
    {
    }

    void add(int frame_id, std::vector<Packet>&& video_packets, std::vector<Packet>&& parity_packets)
    {
        video_frame_packets_.insert({frame_id, VideoFramePackets(std::move(video_packets), std::move(parity_packets))});
    }

    auto find(int frame_id)
    {
        return video_frame_packets_.find(frame_id);
    }

    auto end()
    {
        return video_frame_packets_.end();
    }

    void cleanup(int min_receiver_frame_id)
    {
        for (auto it{video_frame_packets_.begin()}; it != video_frame_packets_.end();) {
            if (it->first <= min_receiver_frame_id) {
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