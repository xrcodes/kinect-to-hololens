#pragma once

#include <optional>
#include "kh_video_message.h"
#include "kh_time.h"
#include "kh_packet.h"

namespace kh
{
class VideoPacketCollection
{
public:
    VideoPacketCollection(int frame_id, int packet_count);
    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }
    std::vector<std::optional<VideoSenderPacketData>>& packet_data_set() { return packet_data_set_; }
    void addPacketData(int packet_index, VideoSenderPacketData&& packet_data);
    bool isFull();
    //VideoMessage toMessage();
    int getCollectedPacketCount();
    std::vector<int> getMissingPacketIndices();

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::optional<VideoSenderPacketData>> packet_data_set_;
    TimePoint construction_time_;
};
}