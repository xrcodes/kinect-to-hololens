#include "kh_fec_packet_collection.h"

namespace kh
{
FecPacketCollection::FecPacketCollection(int frame_id, int packet_count)
    : frame_id_(frame_id), packet_count_(packet_count), packet_data_vector_(packet_count)
{
}

void FecPacketCollection::addPacket(int packet_index, FecSenderPacketData&& packet)
{
    packet_data_vector_[packet_index] = std::move(packet);
}

const std::optional<FecSenderPacketData>& FecPacketCollection::TryGetPacket(int packet_index)
{
    return packet_data_vector_[packet_index];
}
}