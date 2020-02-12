#include "kh_frame_packet_collection.h"

#include "kh_packet_helper.h"

namespace kh
{
FramePacketCollection::FramePacketCollection(int frame_id, int packet_count)
    : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count_),
    construction_time_(std::chrono::steady_clock::now())
{
}

void FramePacketCollection::addPacket(int packet_index, std::vector<uint8_t>&& packet)
{
    packets_[packet_index] = std::move(packet);
}

bool FramePacketCollection::isFull()
{
    for (auto packet : packets_) {
        if (packet.empty())
            return false;
    }

    return true;
}

FrameMessage FramePacketCollection::toMessage() {
    int message_size = 0;
    for (auto packet : packets_) {
        message_size += packet.size() - KH_PACKET_HEADER_SIZE;
    }

    std::vector<uint8_t> message(message_size);
    for (int i = 0; i < packets_.size(); ++i) {
        int cursor = (KH_PACKET_SIZE - KH_PACKET_HEADER_SIZE) * i;
        memcpy(message.data() + cursor, packets_[i].data() + KH_PACKET_HEADER_SIZE, packets_[i].size() - KH_PACKET_HEADER_SIZE);
    }

    auto packet_collection_time = std::chrono::steady_clock::now() - construction_time_;
    return FrameMessage::create(frame_id_, std::move(message), packet_collection_time);
}

int FramePacketCollection::getCollectedPacketCount() {
    int count = 0;
    for (auto packet : packets_) {
        if (!packet.empty())
            ++count;
    }

    return count;
}

std::vector<int> FramePacketCollection::getMissingPacketIndices()
{
    std::vector<int> missing_packet_indices;
    for (int i = 0; i < packets_.size(); ++i) {
        if (packets_[i].empty())
            missing_packet_indices.push_back(i);
    }
    return missing_packet_indices;
}
}