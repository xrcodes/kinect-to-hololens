#include "kh_xor_packet_collection.h"

namespace kh
{
XorPacketCollection::XorPacketCollection(int frame_id, int packet_count)
    : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count)
{
}

void XorPacketCollection::addPacket(int packet_index, std::vector<std::byte>&& packet)
{
    packets_[packet_index] = std::move(packet);
}

std::vector<std::byte>* XorPacketCollection::TryGetPacket(int packet_index)
{
    if (packets_[packet_index].empty())
    {
        return nullptr;
    }

    return &packets_[packet_index];
}
}