#pragma once

#include <asio.hpp>
#include <optional>
#include "k4a/k4a.hpp"

namespace kh
{
class Sender
{
public:
    Sender(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint, int send_buffer_size);
    void send(k4a_calibration_t calibration);
    void send(int frame_id, float frame_time_stamp, bool keyframe, std::vector<uint8_t>& vp8_frame,
              uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size);
    std::optional<std::vector<uint8_t>> receive();

private:
    static std::vector<uint8_t> createFrameMessage(float frame_time_stamp, bool keyframe, std::vector<uint8_t>& vp8_frame,
                                            uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size);
    static std::vector<std::vector<uint8_t>> splitFrameMessage(int frame_id, std::vector<uint8_t> frame_message);
    void sendPacket(const std::vector<uint8_t>& packet);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}