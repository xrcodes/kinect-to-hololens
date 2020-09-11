#pragma once

#include <unordered_map>

namespace kh
{
struct RemoteReceiver
{
    // The video frame ID before any report from the receiver.
    const asio::ip::udp::endpoint endpoint;
    const int receiver_id;
    bool video_requested;
    bool audio_requested;
    std::optional<int> video_frame_id;
    tt::TimePoint last_packet_time;

    RemoteReceiver(asio::ip::udp::endpoint endpoint, int receiver_id, bool video_requested, bool audio_requested)
        : endpoint{endpoint}
        , receiver_id{receiver_id}
        , video_requested{video_requested}
        , audio_requested{audio_requested}
        , video_frame_id{std::nullopt}
        , last_packet_time{tt::TimePoint::now()}
    {
    }
};
}