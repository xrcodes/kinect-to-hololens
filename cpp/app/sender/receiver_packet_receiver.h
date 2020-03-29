#pragma once

#include <readerwriterqueue/readerwriterqueue.h>
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"

namespace kh
{
class ReceiverPacketReceiver
{
public:
    ReceiverPacketReceiver()
        : connect_endpoint_queue_{}, report_packet_data_queue_ {}, request_packet_data_queue_{}
    {
    }

    void receive(UdpSocket& udp_socket)
    {
        while (auto packet{udp_socket.receive()}) {
            switch (get_packet_type_from_receiver_packet_bytes(packet->bytes))
            {
            case ReceiverPacketType::Connect:
                connect_endpoint_queue_.enqueue(packet->endpoint);
                break;
            case ReceiverPacketType::Report:
                report_packet_data_queue_.enqueue(parse_report_receiver_packet_bytes(packet->bytes));
                break;
            case ReceiverPacketType::Request:
                request_packet_data_queue_.enqueue(parse_request_receiver_packet_bytes(packet->bytes));
                break;
            }
        }
    }

    moodycamel::ReaderWriterQueue<asio::ip::udp::endpoint>& connect_endpoint_queue() { return connect_endpoint_queue_; }
    moodycamel::ReaderWriterQueue<ReportReceiverPacketData>& report_packet_data_queue() { return report_packet_data_queue_; }
    moodycamel::ReaderWriterQueue<RequestReceiverPacketData>& request_packet_data_queue() { return request_packet_data_queue_; }

private:
    moodycamel::ReaderWriterQueue<asio::ip::udp::endpoint> connect_endpoint_queue_;
    moodycamel::ReaderWriterQueue<ReportReceiverPacketData> report_packet_data_queue_;
    moodycamel::ReaderWriterQueue<RequestReceiverPacketData> request_packet_data_queue_;
};
}