#include "kh_receiver_socket.h"

#include <iostream>
#include "kh_packet.h"

namespace kh
{
ReceiverSocket::ReceiverSocket(asio::io_context& io_context, asio::ip::udp::endpoint remote_endpoint, int receive_buffer_size)
    : socket_{io_context}, remote_endpoint_{remote_endpoint}
{
    socket_.open(asio::ip::udp::v4());
    socket_.non_blocking(true);
    socket_.set_option(asio::socket_base::receive_buffer_size{receive_buffer_size});
}

std::optional<std::vector<std::byte>> ReceiverSocket::receive(std::error_code& error)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    asio::ip::udp::endpoint sender_endpoint;
    size_t packet_size{socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, error)};

    if (error)
        return std::nullopt;

    packet.resize(packet_size);
    return packet;
}

void ReceiverSocket::send(gsl::span<const std::byte> bytes, std::error_code& error)
{
    socket_.send_to(asio::buffer(bytes.data(), bytes.size()), remote_endpoint_, 0, error);
}
}