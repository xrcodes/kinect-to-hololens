#pragma once

#include <iostream>
#include "native/tt_native.h"

namespace kh
{
struct ConnectPacketInfo
{
    asio::ip::udp::endpoint receiver_endpoint;
    tt::ConnectReceiverPacket connect_packet;
};

struct ReceiverPacketInfo
{
    bool received_any{false};
    std::vector<tt::ReportReceiverPacket> report_packets;
    std::vector<tt::RequestReceiverPacket> request_packets;
};

struct ReceiverPacketCollection
{
    std::vector<ConnectPacketInfo> connect_packet_infos;
    std::map<int, ReceiverPacketInfo> receiver_packet_infos;
};

class ReceiverPacketClassifier
{
public:
    static ReceiverPacketCollection classify(tt::UdpSocket& udp_socket, std::map<int, RemoteReceiver>& remote_receivers)
    {
        // Prepare ReceiverPacketCollection in regard with the list of RemoteReceivers.
        ReceiverPacketCollection receiver_packet_collection;
        for (auto& [receiver_id, _] : remote_receivers)
            receiver_packet_collection.receiver_packet_infos.insert({receiver_id, ReceiverPacketInfo{}});

        // Iterate through all received UDP packets.
        while (auto packet{udp_socket.receive(tt::KH_PACKET_SIZE)}) {
            // After an update with vcpkg, there started to be some zero-length datagrams arriving.
            // Since they were never sent, I suspect this is a bug (or a new feature) from the newer version of asio, but not sure.
            // TODO: Fix this in the right way.
            if (packet->bytes.size() == 0) {
                std::cout << "ReceiverPacketClassifier found a zero-length packet.\n";
                continue;
            }

            int receiver_id{tt::get_receiver_id_from_receiver_packet_bytes(packet->bytes)};
            auto packet_type{tt::get_packet_type_from_receiver_packet_bytes(packet->bytes)};

            // Collect attempts from recievers to connect.
            if (packet_type == tt::ReceiverPacketType::Connect) {
                receiver_packet_collection.connect_packet_infos.push_back({packet->endpoint,
                                                                           tt::read_connect_receiver_packet(packet->bytes)});
                continue;
            }

            // Skip a packet, not for connection, is not from a reciever already connected.
            auto receiver_packet_set_it{receiver_packet_collection.receiver_packet_infos.find(receiver_id)};
            if (receiver_packet_set_it == receiver_packet_collection.receiver_packet_infos.end())
                continue;

            receiver_packet_set_it->second.received_any = true;
            switch (packet_type) {
            case tt::ReceiverPacketType::Heartbeat:
                break;
            case tt::ReceiverPacketType::Report:
                receiver_packet_set_it->second.report_packets.push_back(tt::read_report_receiver_packet(packet->bytes));
                break;
            case tt::ReceiverPacketType::Request:
                receiver_packet_set_it->second.request_packets.push_back(tt::read_request_receiver_packet(packet->bytes));
                break;
            }
        }

        return receiver_packet_collection;
    }
};
}
