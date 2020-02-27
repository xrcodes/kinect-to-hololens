#include "kh_video_packet_collection.h"

#include "kh_packet.h"

namespace kh
{
VideoPacketCollection::VideoPacketCollection(int frame_id, int packet_count)
    : frame_id_(frame_id), packet_count_(packet_count), packet_data_set_(packet_count_),
    construction_time_(TimePoint::now())
{
}

void VideoPacketCollection::addPacketData(int packet_index, VideoSenderPacketData&& packet_data)
{
    packet_data_set_[packet_index] = std::move(packet_data);
}

bool VideoPacketCollection::isFull()
{
    for (auto& packet_data : packet_data_set_) {
        if (!packet_data)
            return false;
    }

    return true;
}

VideoMessage VideoPacketCollection::toMessage() {
    int message_size{0};
    for (auto& packet_data : packet_data_set_) {
        message_size += packet_data->message_data.size();
    }

    std::vector<std::byte> message(message_size);
    int cursor{0};
    for (auto& packet_data : packet_data_set_) {
        memcpy(message.data() + cursor, packet_data->message_data.data(), packet_data->message_data.size());
        cursor += packet_data->message_data.size();
    }

    auto packet_collection_time = TimePoint::now() - construction_time_;
    return VideoMessage::create(frame_id_, std::move(message), packet_collection_time);
}

int VideoPacketCollection::getCollectedPacketCount() {
    int count = 0;
    for (auto& packet_data : packet_data_set_) {
        if (packet_data)
            ++count;
    }

    return count;
}

std::vector<int> VideoPacketCollection::getMissingPacketIndices()
{
    std::vector<int> missing_packet_indices;
    for (int i = 0; i < packet_data_set_.size(); ++i) {
        if (!packet_data_set_[i])
            missing_packet_indices.push_back(i);
    }
    return missing_packet_indices;
}
}