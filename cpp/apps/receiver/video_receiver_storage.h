#pragma once

#include "native/kh_native.h"
#include "video_message_queue.h"

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

struct MissingPackets
{
    int frame_id;
    std::vector<int> video_packet_indices;
    std::vector<int> parity_packet_indices;
};

// A PacketParityGroup contains VideoSenderPackets with packet_index
// from min_video_packet_index to (min_video_packet_index + group_size - 1).
struct PacketParityGroup
{
    enum class State
    {
        Incorrect, Correctable, Correct
    };

    int min_video_packet_index;
    std::vector<std::shared_ptr<VideoSenderPacket>> video_packets;
    std::shared_ptr<ParitySenderPacket> parity_packet;

    PacketParityGroup(int min_video_packet_index, std::vector<std::shared_ptr<VideoSenderPacket>>::size_type group_size)
        : min_video_packet_index{min_video_packet_index}, video_packets(group_size, nullptr), parity_packet{nullptr}
    {
    }

    void addVideoPacket(std::shared_ptr<VideoSenderPacket> packet)
    {
        int vector_index{packet->packet_index - min_video_packet_index};
        video_packets[vector_index] = packet;
    }

    void setParityPacket(std::shared_ptr<ParitySenderPacket> packet)
    {
        parity_packet = packet;
    }

    State getState()
    {
        auto group_size{video_packets.size()};
        int packet_count{0};
        for (auto& video_packet : video_packets) {
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

    void correct()
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
        for (auto& correct_video_packet : correct_video_packets) {
            bytes_ptrs.push_back(&correct_video_packet->message_data);
        }
        bytes_ptrs.push_back(&parity_packet->bytes);

        int corrected_sender_id{parity_packet->sender_id};
        SenderPacketType corrected_type{SenderPacketType::Video};
        int corrected_frame_id{parity_packet->frame_id};
        int corrected_packet_index{min_video_packet_index + incorrect_vector_index};
        int corrected_packet_count{parity_packet->video_packet_count};
        
        video_packets[incorrect_vector_index].reset(new VideoSenderPacket{corrected_sender_id,
                                                                          corrected_type,
                                                                          corrected_frame_id,
                                                                          corrected_packet_index,
                                                                          corrected_packet_count,
                                                                          xor (bytes_ptrs)});
    }
};

struct FrameParitySet
{
    enum class State
    {
        Incorrect, Correctable, Correct
    };

    // Index is the index of the parity packet of the group.
    std::vector<std::optional<PacketParityGroup>> packet_parity_groups;

    FrameParitySet(std::vector<std::optional<PacketParityGroup>>::size_type packet_parity_set_count)
        : packet_parity_groups(packet_parity_set_count, std::nullopt)
    {
    }

    void addVideoPacket(std::shared_ptr<VideoSenderPacket> video_packet)
    {
        int parity_index{video_packet->packet_index / KH_FEC_GROUP_SIZE};
        if (!packet_parity_groups[parity_index]) {
            int min_video_packet_index{parity_index * KH_FEC_GROUP_SIZE};
            // The last group can have a group size smaller than KH_FEC_GROUP_SIZE.
            int group_size{std::min<int>(KH_FEC_GROUP_SIZE, video_packet->packet_count - min_video_packet_index)};
            packet_parity_groups[parity_index] = PacketParityGroup{min_video_packet_index, group_size};
        }

        packet_parity_groups[parity_index]->addVideoPacket(video_packet);
    }

    void addParityPacket(std::shared_ptr<ParitySenderPacket> parity_packet)
    {
        if (!packet_parity_groups[parity_packet->packet_index]) {
            int min_video_packet_index{parity_packet->packet_index * KH_FEC_GROUP_SIZE};
            // The last group can have a group size smaller than KH_FEC_GROUP_SIZE.
            int group_size{std::min<int>(KH_FEC_GROUP_SIZE, parity_packet->video_packet_count - min_video_packet_index)};
            packet_parity_groups[parity_packet->packet_index] = PacketParityGroup{min_video_packet_index, group_size};
        }
        
        packet_parity_groups[parity_packet->packet_index]->setParityPacket(parity_packet);
    }

    State getState()
    {
        int incorrect_count{0};
        int correct_count{0};
        for (auto& group : packet_parity_groups) {
            switch (group->getState()) {
            case PacketParityGroup::State::Incorrect:
                ++incorrect_count;
                break;
            case PacketParityGroup::State::Correct:
                ++correct_count;
                break;
            }
        }

        if (correct_count == packet_parity_groups.size())
            return State::Correct;

        if (incorrect_count == 0)
            return State::Correctable;

        return State::Incorrect;
    }

    // Correct a Correctable set into a Correct set.
    void correct()
    {
        if (getState() != State::Correctable)
            throw std::runtime_error("FrameParitySet::correct() called while set's state is not Correctable.");

        for (auto& group : packet_parity_groups) {
            if (group->getState() == PacketParityGroup::State::Correctable)
                group->correct();
        }
    }

    // Build a message with a Correct set.
    std::unique_ptr<VideoSenderMessage> build()
    {
        if (getState() != State::Correct)
            throw std::runtime_error("FrameParitySet::build() called while set's state is not Correct.");

        std::vector<VideoSenderPacket*> video_packet_ptrs;
        for (auto& group : packet_parity_groups) {
            for (auto& packet : group->video_packets) {
                video_packet_ptrs.push_back(packet.get());
            }
        }

        return std::make_unique<VideoSenderMessage>(read_video_sender_message(merge_video_sender_packets(video_packet_ptrs)));
    }

    // Report indices of missing packets from the Incorrect groups.
    std::pair<std::vector<int>, std::vector<int>> getMissingPackets()
    {
        if (getState() != State::Correct)
            throw std::runtime_error("FrameParitySet::getMissingPackets() called while set's state is not Incorrect.");

        std::vector<int> video_packet_indices;
        std::vector<int> parity_packet_indices;

        for (auto& group : packet_parity_groups) {
            if (group->getState() == PacketParityGroup::State::Incorrect) {
                for (int i{0}; i < group->video_packets.size(); ++i) {
                    if (!group->video_packets[i])
                        video_packet_indices.push_back(group->min_video_packet_index + i);

                    if (!group->parity_packet)
                        parity_packet_indices.push_back(group->min_video_packet_index / KH_FEC_GROUP_SIZE);
                }
            }

        }

        return {video_packet_indices, parity_packet_indices};
    }
};

class VideoReceiverStorage
{
public:
    VideoReceiverStorage()
        : frame_parity_sets_{}
    {
    }

    void addVideoPacket(std::shared_ptr<VideoSenderPacket> video_packet)
    {
        auto frame_parity_set_it{frame_parity_sets_.find(video_packet->frame_id)};
        if (frame_parity_set_it == frame_parity_sets_.end()) {
            const auto parity_packet_count{(video_packet->packet_count - 1) / KH_FEC_GROUP_SIZE + 1};
            std::tie(frame_parity_set_it, std::ignore) = frame_parity_sets_.insert({video_packet->frame_id, FrameParitySet{parity_packet_count}});
        }
        
        frame_parity_set_it->second.addVideoPacket(video_packet);
    }

    void addParityPacket(std::shared_ptr<ParitySenderPacket> parity_packet)
    {
        auto frame_parity_set_it{frame_parity_sets_.find(parity_packet->frame_id)};
        if (frame_parity_set_it == frame_parity_sets_.end()) {
            const auto parity_packet_count{(parity_packet->video_packet_count - 1) / KH_FEC_GROUP_SIZE + 1};
            std::tie(frame_parity_set_it, std::ignore) = frame_parity_sets_.insert({parity_packet->frame_id, FrameParitySet{parity_packet_count}});
        }
        
        frame_parity_set_it->second.addParityPacket(parity_packet);
    }

    // Add video messages as much as possible to the queue.
    void build(VideoMessageQueue& message_queue)
    {
        for (auto& set_pair : frame_parity_sets_) {
            auto set_state{set_pair.second.getState()};
            if (set_state == FrameParitySet::State::Correctable)
                set_pair.second.correct();

            if (set_pair.second.getState() != FrameParitySet::State::Incorrect)
                message_queue.messages.push_back({set_pair.first, set_pair.second.build()});
        }
    }

    // To check whether a packet for a new frame arrived to trigger retransmission.
    int getMaxFrameId()
    {
        int max_frame_id{INT_MIN};
        for (auto& set_pair : frame_parity_sets_) {
            if (set_pair.first > max_frame_id)
                max_frame_id = set_pair.first;
        }
        return max_frame_id;
    }

    // Should not request retransmission for the frame of the new packet, since other packets are already comming.
    // Set max_frame_id lower than the newest frame that triggered this call of getMissingPackets().
    std::vector<MissingPackets> getMissingPackets(int max_frame_id)
    {
        std::vector<MissingPackets> missing_packets_vector;
        for (auto& [frame_id, frame_parity_sets]: frame_parity_sets_) {
            if (frame_id <= max_frame_id) {
                if (frame_parity_sets.getState() == FrameParitySet::State::Incorrect) {
                    auto [video_packet_indices, parity_packet_indices] {frame_parity_sets.getMissingPackets()};

                    missing_packets_vector.emplace_back(frame_id, video_packet_indices, parity_packet_indices);
                }
            }
        }

        return missing_packets_vector;
    }

    void removeObsolete(int last_frame_id)
    {
        for (auto it = frame_parity_sets_.begin(); it != frame_parity_sets_.end();) {
            if (it->first <= last_frame_id) {
                it = frame_parity_sets_.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    // Key is frame ID.
    std::map<int, FrameParitySet> frame_parity_sets_;
};
}