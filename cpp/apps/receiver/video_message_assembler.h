#pragma once

#include <map>
#include "video_renderer_state.h"

namespace kh
{
class VideoMessageAssembler
{
public:
    VideoMessageAssembler(const int session_id, const asio::ip::udp::endpoint remote_endpoint);
    void assemble(UdpSocket& udp_socket,
                  std::vector<VideoSenderPacket>& video_packet_data_vector,
                  std::vector<ParitySenderPacket>& parity_packet_data_vector,
                  VideoRendererState video_renderer_state,
                  std::map<int, VideoSenderMessageData>& video_frame_messages);

private:
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;
    std::unordered_map<int, std::vector<std::optional<VideoSenderPacket>>> video_packet_collections_;
    std::unordered_map<int, std::vector<std::optional<ParitySenderPacket>>> parity_packet_collections_;
};
}