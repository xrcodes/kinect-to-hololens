#pragma once


#include <optional>

// _WIN32_WINNT is for asio.
#define _WIN32_WINNT _WIN32_WINNT_WIN10
#pragma warning(push)
#pragma warning(disable: 4996)
#pragma warning(disable: 6001)
#pragma warning(disable: 6031)
#pragma warning(disable: 6255)
#pragma warning(disable: 6258)
#pragma warning(disable: 6319)
#pragma warning(disable: 6387)
#pragma warning(disable: 26439)
#pragma warning(disable: 26495)
#pragma warning(disable: 26812)
#include <asio.hpp>
#pragma warning(pop)

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
    UdpSocketRuntimeError(const std::string& message, std::error_code asio_error_code, asio::ip::udp::endpoint endpoint)
        : std::runtime_error(message), asio_error_code_{asio_error_code}, endpoint_{endpoint}
    {
    }
    std::error_code asio_error_code() { return asio_error_code_; }
    asio::ip::udp::endpoint endpoint() { return endpoint_; }

private:
    std::error_code asio_error_code_;
    asio::ip::udp::endpoint endpoint_;
};

class UdpSocket
{
public:
    UdpSocket(asio::ip::udp::socket&& socket);
    ~UdpSocket();
    std::optional<UdpSocketPacket> receive();
    void send(gsl::span<const std::byte> bytes, asio::ip::udp::endpoint endpoint);

private:
    asio::ip::udp::socket socket_;
};
}