#pragma once

#include <asio.hpp>
#include <optional>

namespace kh
{
class Receiver
{
public:
    Receiver(asio::io_context& io_context, int receive_buffer_size);
    void ping(std::string ip_address, int port);
    std::optional<std::vector<uint8_t>> receive();
    void send(int frame_id, float packet_collection_time_ms, float decoder_time_ms,
              float frame_time_ms);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}