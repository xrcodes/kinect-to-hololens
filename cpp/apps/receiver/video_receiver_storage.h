#pragma once

#include "native/kh_native.h"

namespace kh
{
namespace
{
std::vector<std::byte> xor(gsl::span<std::vector<std::byte>*> bytes_ptrs)
{
    std::vector<std::byte> result(*bytes_ptrs[0]);
    for (gsl::index i{1}; i < bytes_ptrs.size(); ++i) {
        auto bytes_ptr{bytes_ptrs[i]};
        if (result.size() != bytes_ptr->size())
            throw std::runtime_error("Size mismatch between result and bytes_ptr in xor().");
        for (gsl::index j{0}; j < result.size(); ++j)
            result[j] ^= bytes_ptr->at(j);
    }
    return result;
}
}

struct PacketParityGroup
{
    enum class State
    {
        Incorrect, Correctable, Correct
    };

    int first_video_packet_index;
    std::vector<std::shared_ptr<VideoSenderPacket>> video_packets;
    std::shared_ptr<ParitySenderPacket> parity_packet;

    PacketParityGroup(int first_video_packet_index, std::vector<std::shared_ptr<VideoSenderPacket>>::size_type group_size)
        : first_video_packet_index{first_video_packet_index}, video_packets(group_size, nullptr), parity_packet{nullptr}
    {
    }

    State getState()
    {
        auto group_size{video_packets.size()};
        int packet_count{0};
        for (auto video_packet : video_packets) {
            if (video_packet)
                ++packet_count;
        }
        if (packet_count == group_size)
            return State::Correct;

        if (parity_packet)
            ++packet_count;
        
        if (packet_count == group_size)
            return State::Correctable;
        
        return State::Incorrect;
    }

    void correct(int video_packet_count)
    {
        if (getState() != State::Correctable)
            throw std::runtime_error("PacketParityGroup::correct() called while group's state is not Correctable.");

        gsl::index incorrect_vector_index;
        std::vector<std::shared_ptr<VideoSenderPacket>> correct_video_packets;
        for (gsl::index i{0}; i < video_packets.size(); ++i) {
            if (!video_packets[i]) {
                incorrect_vector_index = i;
            } else {
                correct_video_packets.push_back(video_packets[i]);
            }
        }

        // video_packets.size() == group_size
        std::vector<std::vector<std::byte>*> bytes_ptrs{video_packets.size()};
        for (auto correct_video_packet : correct_video_packets) {
            bytes_ptrs.push_back(&correct_video_packet->message_data);
        }
        bytes_ptrs.push_back(&parity_packet->bytes);

        int corrected_sender_id{parity_packet->sender_id};
        SenderPacketType corrected_type{SenderPacketType::Video};
        int corrected_frame_id{parity_packet->frame_id};
        int corrected_packet_index{first_video_packet_index + incorrect_vector_index};
        int corrected_packet_count{video_packet_count};
        auto corrected_message_data{xor(bytes_ptrs)};
    }
};

struct FrameParitySet
{
    enum class State
    {
        Incorrect, Correctable, Correct
    };

    std::vector<std::optional<PacketParityGroup>> packet_parity_groups;

    FrameParitySet(std::vector<std::optional<PacketParityGroup>>::size_type packet_parity_set_count)
        : packet_parity_groups(packet_parity_set_count, std::nullopt)
    {
    }
};

class VideoReceiverStorage
{
public:
    VideoReceiverStorage()
    {
    };

private:

};
}