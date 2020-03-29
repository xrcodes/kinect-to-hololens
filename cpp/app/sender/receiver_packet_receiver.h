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
        : report_packet_data_queue_{}, request_packet_data_queue_{}
    {
    }

    void receive(UdpSocket& udp_socket)
    {
        while (auto packet{udp_socket.receive()}) {
            switch (get_packet_type_from_receiver_packet_bytes(packet->bytes))
            {
            case ReceiverPacketType::Report:
                report_packet_data_queue_.enqueue(parse_report_receiver_packet_bytes(packet->bytes));
                break;
            case ReceiverPacketType::Request:
                request_packet_data_queue_.enqueue(parse_request_receiver_packet_bytes(packet->bytes));
                break;
            }
        }
    }

    moodycamel::ReaderWriterQueue<ReportReceiverPacketData>& report_packet_data_queue() { return report_packet_data_queue_; }
    moodycamel::ReaderWriterQueue<RequestReceiverPacketData>& request_packet_data_queue() { return request_packet_data_queue_; }

private:
    moodycamel::ReaderWriterQueue<ReportReceiverPacketData> report_packet_data_queue_;
    moodycamel::ReaderWriterQueue<RequestReceiverPacketData> request_packet_data_queue_;
};
}