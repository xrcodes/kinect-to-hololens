#include "kh_video_packet_collection.h"

#include "kh_packet.h"

namespace kh
{
VideoPacketCollection::VideoPacketCollection(int frame_id, int packet_count)
    : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count_),
    construction_time_(TimePoint::now())
{
}

void VideoPacketCollection::addPacket(int packet_index, std::vector<std::byte>&& packet)
{
    packets_[packet_index] = std::move(packet);
}

bool VideoPacketCollection::isFull()
{
    for (auto packet : packets_) {
        if (packet.empty())
            return false;
    }

    return true;
}

VideoMessage VideoPacketCollection::toMessage() {
    int message_size = 0;
    for (auto packet : packets_) {
        message_size += packet.size() - KH_VIDEO_PACKET_HEADER_SIZE;
    }

    std::vector<uint8_t> message(message_size);
    for (int i = 0; i < packets_.size(); ++i) {
        int cursor = (KH_PACKET_SIZE - KH_VIDEO_PACKET_HEADER_SIZE) * i;
        memcpy(message.data() + cursor, packets_[i].data() + KH_VIDEO_PACKET_HEADER_SIZE, packets_[i].size() - KH_VIDEO_PACKET_HEADER_SIZE);
    }

    auto packet_collection_time = TimePoint::now() - construction_time_;
    return VideoMessage::create(frame_id_, std::move(message), packet_collection_time);
}

int VideoPacketCollection::getCollectedPacketCount() {
    int count = 0;
    for (auto packet : packets_) {
        if (!packet.empty())
            ++count;
    }

    return count;
}

std::vector<int> VideoPacketCollection::getMissingPacketIndices()
{
    std::vector<int> missing_packet_indices;
    for (int i = 0; i < packets_.size(); ++i) {
        if (packets_[i].empty())
            missing_packet_indices.push_back(i);
    }
    return missing_packet_indices;
}
}