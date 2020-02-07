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
    void sendInitPacket(int session_id, k4a_calibration_t calibration);
    std::optional<std::vector<uint8_t>> receive();
    static std::vector<uint8_t> createFrameMessage(float frame_time_stamp, bool keyframe, std::vector<uint8_t>& vp8_frame,
                                            uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size);
    static std::vector<std::vector<uint8_t>> createFramePackets(int session_id, int frame_id, const std::vector<uint8_t>& frame_message);
    static std::vector<std::vector<uint8_t>> createXorPackets(int session_id, int frame_id,
                                                              const std::vector<std::vector<uint8_t>>& packets, int max_group_size);
    void sendPacket(const std::vector<uint8_t>& packet);

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}