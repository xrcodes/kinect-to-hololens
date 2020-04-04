#pragma once

#include <vector>

using Bytes = std::vector<std::byte>;
constexpr static int XOR_MAX_GROUP_SIZE{5};

struct ReceiverState
{
    // The video frame ID before any report from the receiver.
    static constexpr int INITIAL_VIDEO_FRAME_ID{-1};
    int video_frame_id{INITIAL_VIDEO_FRAME_ID};
};

// This class includes both video packet bytes and fec packet bytes
// for a video frame.
struct VideoFecPacketByteSet
{
    int frame_id;
    std::vector<std::vector<std::byte>> video_packet_byte_set;
    std::vector<std::vector<std::byte>> fec_packet_byte_set;

    VideoFecPacketByteSet(int frame_id,
                          std::vector<std::vector<std::byte>> video_packet_byte_set,
                          std::vector<std::vector<std::byte>> fec_packet_byte_set)
        : frame_id{frame_id}, video_packet_byte_set{video_packet_byte_set}, fec_packet_byte_set{fec_packet_byte_set}
    {
    }

    VideoFecPacketByteSet()
        : frame_id{0}, video_packet_byte_set{}, fec_packet_byte_set{}
    {
    }
};