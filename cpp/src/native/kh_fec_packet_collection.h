#pragma once

#include <optional>
#include <vector>
#include "kh_packet.h"

namespace kh
{
class FecPacketCollection
{
public:
    FecPacketCollection(int frame_id, int packet_count);
    int frame_id() { return frame_id_; }
    int packet_count() { return packet_count_; }
    void addPacketData(int packet_index, FecSenderPacketData&& packet);
    const std::optional<FecSenderPacketData>& GetPacketData(int packet_index);

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::optional<FecSenderPacketData>> packet_data_set_;
};
}