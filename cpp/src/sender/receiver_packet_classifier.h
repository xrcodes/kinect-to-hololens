#pragma once

#include "native/kh_native.h"

namespace kh
{
struct ConnectPacketInfo
{
    asio::ip::udp::endpoint receiver_endpoint;
    ConnectReceiverPacket connect_packet;
};

struct ReceiverPacketInfo
{
    bool received_any{false};
    std::vector<ReportReceiverPacket> report_packets;
    std::vector<RequestReceiverPacket> request_packets;
};

struct ReceiverPacketCollection
{
    std::vector<ConnectPacketInfo> connect_packet_infos;
    std::unordered_map<int, ReceiverPacketInfo> receiver_packet_infos;
};

class ReceiverPacketClassifier
{
public:
    static ReceiverPacketCollection categorizePackets(UdpSocket& udp_socket, std::unordered_map<int, RemoteReceiver>& remote_receivers)
    {
        // Prepare ReceiverPacketCollection in regard with the list of RemoteReceivers.
        ReceiverPacketCollection receiver_packet_collection;
        for (auto& [receiver_id, _] : remote_receivers)
            receiver_packet_collection.receiver_packet_infos.insert({receiver_id, ReceiverPacketInfo{}});

        // Iterate through all received UDP packets.
        while (auto packet{udp_socket.receive()}) {
            int receiver_id{get_receiver_id_from_receiver_packet_bytes(packet->bytes)};
            auto packet_type{get_packet_type_from_receiver_packet_bytes(packet->bytes)};

            // Collect attempts from recievers to connect.
            if (packet_type == ReceiverPacketType::Connect) {
                receiver_packet_collection.connect_packet_infos.push_back({packet->endpoint,
                                                                           read_connect_receiver_packet(packet->bytes)});
                continue;
            }

            // Skip a packet, not for connection, is not from a reciever already connected.
            auto receiver_packet_set_ref{receiver_packet_collection.receiver_packet_infos.find(receiver_id)};
            if (receiver_packet_set_ref == receiver_packet_collection.receiver_packet_infos.end())
                continue;

            receiver_packet_set_ref->second.received_any = true;
            switch (packet_type) {
            case ReceiverPacketType::Heartbeat:
                break;
            case ReceiverPacketType::Report:
                receiver_packet_set_ref->second.report_packets.push_back(read_report_receiver_packet(packet->bytes));
                break;
            case ReceiverPacketType::Request:
                receiver_packet_set_ref->second.request_packets.push_back(read_request_receiver_packet(packet->bytes));
                break;
            }
        }

        return receiver_packet_collection;
    }
};
}
