#pragma once

#include <asio.hpp>
#include <optional>
#include <gsl/gsl>
#include <k4a/k4a.hpp>

namespace kh
{
class SenderSocket
{
public:
    SenderSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint, int send_buffer_size);
    std::optional<std::vector<std::byte>> receive(std::error_code& error);
    void send(gsl::span<const std::byte> bytes, std::error_code& error);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}