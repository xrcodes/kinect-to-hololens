#pragma once

#include <optional>
#include <asio.hpp>
#include <gsl/gsl>

namespace kh
{
class UdpSocket
{
public:
    UdpSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint);
    std::optional<std::vector<std::byte>> receive(std::error_code& error);
    void send(gsl::span<const std::byte> bytes, std::error_code& error);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}