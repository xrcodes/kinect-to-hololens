#pragma once

#include <vector>

namespace kh
{
class FecPacketCollection
{
public:
    FecPacketCollection(int frame_id, int packet_count);
    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }
    void addPacket(int packet_index, std::vector<std::byte>&& packet);
    std::vector<std::byte>* TryGetPacket(int packet_index);

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::vector<std::byte>> packets_;
};
}