#include "kh_sender_socket.h"

#include <algorithm>
#include <iostream>
#include "kh_packet.h"

namespace kh
{
SenderSocket::SenderSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint,
                     int send_buffer_size)
    : socket_{std::move(socket)}, remote_endpoint_{remote_endpoint}
{
    socket_.non_blocking(true);
    socket_.set_option(asio::socket_base::send_buffer_size{send_buffer_size});
}

std::optional<std::vector<std::byte>> SenderSocket::receive(std::error_code& error)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    asio::ip::udp::endpoint sender_endpoint;
    const size_t packet_size{socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, error)};

    if (error)
        return std::nullopt;

    packet.resize(packet_size);
    return packet;
}

void SenderSocket::send(gsl::span<const std::byte> bytes, std::error_code& error)
{
    socket_.send_to(asio::buffer(bytes.data(), bytes.size()), remote_endpoint_, 0, error);
}
}