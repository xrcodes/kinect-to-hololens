#pragma once

#include <unordered_map>

namespace kh
{
struct RemoteReceiver
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};

    const asio::ip::udp::endpoint endpoint;
    const int session_id;
    bool video_requested;
    bool audio_requested;
    bool floor_requested;
    int video_frame_id;
    TimePoint last_packet_time;

    RemoteReceiver(asio::ip::udp::endpoint endpoint, int session_id, bool video_requested, bool audio_requested, bool floor_requested)
        : endpoint{endpoint}
        , session_id{session_id}
        , video_requested{video_requested}
        , audio_requested{audio_requested}
        , floor_requested{floor_requested}
        , video_frame_id{INITIAL_VIDEO_FRAME_ID}
        , last_packet_time{TimePoint::now()}
    {
    }
};
}