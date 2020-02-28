#include "kh_udp_socket.h"

#include "kh_packet.h"

namespace kh
{
UdpSocket::UdpSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint)
    : socket_{std::move(socket)}, remote_endpoint_{remote_endpoint}
{
    socket_.non_blocking(true);
}

std::optional<std::vector<std::byte>> UdpSocket::receive(std::error_code& error)
{
    std::vector<std::byte> packet(KH_PACKET_SIZE);
    asio::ip::udp::endpoint sender_endpoint;
    const size_t packet_size{socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, error)};

    if (error)
        return std::nullopt;

    packet.resize(packet_size);
    return packet;
}

void UdpSocket::send(gsl::span<const std::byte> bytes, std::error_code& error)
{
    socket_.send_to(asio::buffer(bytes.data(), bytes.size()), remote_endpoint_, 0, error);
}
}