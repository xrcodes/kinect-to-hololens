#pragma once

#include "native/kh_native.h"

namespace kh
{
struct ReceiverPacketSet
{
    bool received_any;
    std::vector<asio::ip::udp::endpoint> connect_endpoint_vector;
    std::vector<ReportReceiverPacketData> report_packet_data_vector;
    std::vector<RequestReceiverPacketData> request_packet_data_vector;
};

// This class should stay as a class since it will have additional functionality in the future.
class ReceiverPacketReceiver
{
public:
    static ReceiverPacketSet receive(UdpSocket& udp_socket)
    {
        ReceiverPacketSet receiver_packet_set;
        receiver_packet_set.received_any = false;
        while (auto packet{udp_socket.receive()}) {
            receiver_packet_set.received_any = true;
            switch (get_packet_type_from_receiver_packet_bytes(packet->bytes))
            {
            case ReceiverPacketType::Connect:
                receiver_packet_set.connect_endpoint_vector.push_back(packet->endpoint);
                break;
            case ReceiverPacketType::Report:
                receiver_packet_set.report_packet_data_vector.push_back(parse_report_receiver_packet_bytes(packet->bytes));
                break;
            case ReceiverPacketType::Request:
                receiver_packet_set.request_packet_data_vector.push_back(parse_request_receiver_packet_bytes(packet->bytes));
                break;
            }
        }

        return receiver_packet_set;
    }
};
}