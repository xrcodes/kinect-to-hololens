#include "kh_udp_socket.h"

#include "kh_packet.h"

namespace kh
{
UdpSocket::UdpSocket(asio::ip::udp::socket&& socket)
    : socket_{std::move(socket)}
{
    socket_.non_blocking(true);
}

UdpSocket::~UdpSocket()
{
    socket_.shutdown(asio::socket_base::shutdown_both);
    socket_.close();
}

std::optional<UdpSocketPacket> UdpSocket::receive()
{
    std::vector<std::byte> bytes(KH_PACKET_SIZE);
    asio::ip::udp::endpoint endpoint;
    std::error_code error;
    const size_t packet_size{socket_.receive_from(asio::buffer(bytes), endpoint, 0, error)};

    if (error == asio::error::would_block)
        return std::nullopt;

    if (error)
        throw UdpSocketRuntimeError(std::string("Failed to receive bytes: ") + error.message(), error, endpoint);

    bytes.resize(packet_size);
    return UdpSocketPacket{bytes, endpoint};
}

void UdpSocket::send(gsl::span<const std::byte> bytes, asio::ip::udp::endpoint endpoint)
{
    std::error_code error;
    socket_.send_to(asio::buffer(bytes.data(), bytes.size()), endpoint, 0, error);

    if(error && error != asio::error::would_block)
        throw UdpSocketRuntimeError(std::string("Failed to send bytes: ") + error.message(), error, endpoint);
}
}