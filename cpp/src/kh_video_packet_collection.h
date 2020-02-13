#pragma once

#include <chrono>
#include "kh_video_message.h"

namespace kh
{
class VideoPacketCollection
{
public:
    VideoPacketCollection(int frame_id, int packet_count);
    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }
    std::vector<std::vector<std::uint8_t>>& packets() { return packets_; }
    void addPacket(int packet_index, std::vector<uint8_t>&& packet);
    bool isFull();
    VideoMessage toMessage();
    int getCollectedPacketCount();
    std::vector<int> getMissingPacketIndices();

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::vector<std::uint8_t>> packets_;
    std::chrono::time_point<std::chrono::steady_clock> construction_time_;
};
}