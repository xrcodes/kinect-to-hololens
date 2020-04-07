#pragma once

#include "native/kh_native.h"

namespace kh
{
struct ConnectPacketInfo
{
    asio::ip::udp::endpoint endpoint;
    int session_id;
};

struct ReceiverPacketSet
{
    bool received_any{false};
    std::vector<ReportReceiverPacketData> report_packet_data_vector;
    std::vector<RequestReceiverPacketData> request_packet_data_vector;
};

struct ReceiverPacketCollection
{
    std::vector<ConnectPacketInfo> connect_packet_infos;
    std::unordered_map<int, ReceiverPacketSet> receiver_packet_sets;
};

// This class should stay as a class since it will have additional functionality in the future.
class ReceiverPacketReceiver
{
public:
    static ReceiverPacketCollection receive(UdpSocket& udp_socket, std::vector<int>& receiver_session_ids)
    {
        ReceiverPacketCollection receiver_packet_collection;
        for (int receiver_session_id : receiver_session_ids)
            receiver_packet_collection.receiver_packet_sets.insert({receiver_session_id, ReceiverPacketSet{}});
        while (auto packet{udp_socket.receive()}) {
            int receiver_session_id{get_session_id_from_receiver_packet_bytes(packet->bytes)};
            ReceiverPacketType packet_type{get_packet_type_from_receiver_packet_bytes(packet->bytes)};

            if(packet_type == ReceiverPacketType::Connect) {
                receiver_packet_collection.connect_packet_infos.push_back({packet->endpoint, receiver_session_id});
                continue;
            }

            auto receiver_packet_set_ref{receiver_packet_collection.receiver_packet_sets.find(receiver_session_id)};
            if (receiver_packet_set_ref == receiver_packet_collection.receiver_packet_sets.end())
                continue;

            receiver_packet_set_ref->second.received_any = true;
            switch (packet_type) {
            case ReceiverPacketType::Report:
                receiver_packet_set_ref->second.report_packet_data_vector.push_back(parse_report_receiver_packet_bytes(packet->bytes));
                break;
            case ReceiverPacketType::Request:
                receiver_packet_set_ref->second.request_packet_data_vector.push_back(parse_request_receiver_packet_bytes(packet->bytes));
                break;
            }
        }

        return receiver_packet_collection;
    }
};
}