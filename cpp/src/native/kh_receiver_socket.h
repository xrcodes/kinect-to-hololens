#pragma once

#include <optional>
#include <vector>
#include <asio.hpp>
#include <gsl/gsl>

namespace kh
{
class ReceiverSocket
{
public:
    ReceiverSocket(asio::io_context& io_context, asio::ip::udp::endpoint remote_endpoint, int receive_buffer_size);
    std::optional<std::vector<std::byte>> receive(std::error_code& error);
    void send(gsl::span<const std::byte> bytes, std::error_code& error);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}