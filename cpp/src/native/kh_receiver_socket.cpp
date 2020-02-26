#include "kh_receiver_socket.h"

#include <iostream>
#include "kh_packet.h"

namespace kh
{
ReceiverSocket::ReceiverSocket(asio::io_context& io_context, int receive_buffer_size)
    : socket_{io_context}, remote_endpoint_{}
{
    socket_.open(asio::ip::udp::v4());
    socket_.non_blocking(true);
    socket_.set_option(asio::socket_base::receive_buffer_size{receive_buffer_size});
}

std::optional<std::vector<std::byte>> ReceiverSocket::receive(std::error_code& error)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    size_t packet_size{socket_.receive_from(asio::buffer(packet), remote_endpoint_, 0, error)};

    if (error)
        return std::nullopt;

    packet.resize(packet_size);
    return packet;
}

// Connects to a Sender with the Sender's IP address and port.
void ReceiverSocket::ping(std::string ip_address, unsigned short port)
{
    remote_endpoint_ = asio::ip::udp::endpoint{asio::ip::address::from_string(ip_address), port};
    auto packet_type{ReceiverPacketType::Ping};
    socket_.send_to(asio::buffer(&packet_type, 1), remote_endpoint_);
}

void ReceiverSocket::send(int frame_id, float packet_collection_time_ms, float decoder_time_ms,
                          float frame_time_ms, int packet_count, std::error_code& error)
{
    const auto report_receiver_packet_data{create_report_receiver_packet_data(frame_id, packet_collection_time_ms,
                                                                              decoder_time_ms, frame_time_ms,
                                                                              packet_count)};
    const auto packet_bytes{create_report_receiver_packet_bytes(report_receiver_packet_data)};

    socket_.send_to(asio::buffer(packet_bytes), remote_endpoint_, 0, error);
}

void ReceiverSocket::send(int frame_id, const std::vector<int>& missing_packet_indices, std::error_code& error)
{
    const auto request_receiver_packet_data{create_request_receiver_packet_data(frame_id, missing_packet_indices)};
    const auto packet_bytes{create_request_receiver_packet_bytes(request_receiver_packet_data)};

    socket_.send_to(asio::buffer(packet_bytes), remote_endpoint_, 0, error);
}
}