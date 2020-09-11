#pragma once

#include <map>
#include "native/kh_native.h"

namespace kh
{
class VideoMessageAssembler
{
public:
    VideoMessageAssembler(const int receiver_id, const asio::ip::udp::endpoint remote_endpoint);
    void assemble(UdpSocket& udp_socket,
                  std::vector<VideoSenderPacket>& video_packets,
                  std::vector<ParitySenderPacket>& parity_packets,
                  int last_frame_id,
                  std::map<int, VideoSenderMessage>& video_messages);

private:
    const int receiver_id_;
    const asio::ip::udp::endpoint remote_endpoint_;
    std::unordered_map<int, std::vector<std::optional<VideoSenderPacket>>> video_packet_collections_;
    std::unordered_map<int, std::vector<std::optional<ParitySenderPacket>>> parity_packet_collections_;
};
}