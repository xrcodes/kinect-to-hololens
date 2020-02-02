#include "kh_receiver_udp.h"

#include <iostream>

namespace kh
{
ReceiverUdp::ReceiverUdp(asio::io_context& io_context, int receive_buffer_size)
    : socket_(io_context), remote_endpoint_()
{
    socket_.open(asio::ip::udp::v4());
    socket_.non_blocking(true);
    asio::socket_base::receive_buffer_size option(receive_buffer_size);
    socket_.set_option(option);
}

// Connects to a Sender with the Sender's IP address and port.
void ReceiverUdp::ping(std::string ip_address, int port)
{
    remote_endpoint_ = asio::ip::udp::endpoint(asio::ip::address::from_string(ip_address), port);
    std::array<char, 1> send_buf = { { 0 } };
    socket_.send_to(asio::buffer(send_buf), remote_endpoint_);
}

std::optional<std::vector<uint8_t>> ReceiverUdp::receive()
{
    std::vector<uint8_t> packet(1500);
    std::error_code error;
    size_t packet_size = socket_.receive_from(asio::buffer(packet), remote_endpoint_, 0, error);

    if (error == asio::error::would_block) {
        return std::nullopt;
    } else if (error) {
        std::cout << "Error from ReceiverUdp: " << error.message() << std::endl;
        return std::nullopt;
    }

    packet.resize(packet_size);
    return packet;
}

void ReceiverUdp::send(int frame_id)
{
    std::array<char, 5> frame_id_buffer;
    frame_id_buffer[0] = 1;
    memcpy(frame_id_buffer.data() + 1, &frame_id, 4);
    socket_.send_to(asio::buffer(frame_id_buffer), remote_endpoint_);
}
}