#pragma once

#include <optional>
#include <vector>
#include <asio.hpp>

namespace kh
{
class ReceiverSocket
{
public:
    ReceiverSocket(asio::io_context& io_context, int receive_buffer_size);
    std::optional<std::vector<std::byte>> receive(std::error_code& error);
    void ping(std::string ip_address, unsigned short port);
    void send(int frame_id, float decoder_time_ms,
              float frame_time_ms, int packet_count, std::error_code& error);
    void send(int frame_id, const std::vector<int>& missing_packet_indices, std::error_code& error);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}