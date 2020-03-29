#pragma once

// This is for asio
#define _WIN32_WINNT _WIN32_WINNT_WIN10

#include <optional>
#include <asio.hpp>
#include <gsl/gsl>

namespace kh
{
struct UdpSocketPacket
{
    std::vector<std::byte> bytes;
    asio::ip::udp::endpoint endpoint;
};

class UdpSocketRuntimeError : public std::runtime_error
{
public:
    UdpSocketRuntimeError(const std::string& message, std::error_code asio_error_code)
        : std::runtime_error(message), asio_error_code_{asio_error_code}
    {
    }
    std::error_code asio_error_code() { return asio_error_code_; }

private:
    std::error_code asio_error_code_;
};

class UdpSocket
{
public:
    UdpSocket(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint);
    ~UdpSocket();
    std::optional<UdpSocketPacket> receive();
    void send(gsl::span<const std::byte> bytes);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}