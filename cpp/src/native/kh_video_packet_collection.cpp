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

//VideoMessage VideoPacketCollection::toMessage()
//{
//    std::vector<gsl::span<std::byte>> video_sender_message_data_set(packet_data_set_.size());
//    for (gsl::index i{0}; i < packet_data_set_.size(); ++i)
//        video_sender_message_data_set[i] = gsl::span<std::byte>{packet_data_set_[i]->message_data};
//
//    return VideoMessage::create(frame_id_, merge_video_sender_message_bytes(video_sender_message_data_set));
//}

int VideoPacketCollection::getCollectedPacketCount()
{
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