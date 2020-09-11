#include "video_message_assembler.h"

#include <iostream>

namespace kh
{
VideoMessageAssembler::VideoMessageAssembler(const int receiver_id, const asio::ip::udp::endpoint remote_endpoint)
    : receiver_id_{receiver_id}, remote_endpoint_{remote_endpoint}, video_packet_collections_{}, parity_packet_collections_{}
{
}

void VideoMessageAssembler::assemble(UdpSocket& udp_socket,
                                     std::vector<VideoSenderPacket>& video_packets,
                                     std::vector<ParitySenderPacket>& parity_packets,
                                     int last_frame_id,
                                     std::map<int, VideoSenderMessage>& video_messages)
{
    std::optional<int> added_frame_id{std::nullopt};
    // Collect the received video packets.
    for (auto& video_sender_packet_data : video_packets) {
        if (video_sender_packet_data.frame_id <= last_frame_id)
            continue;

        auto video_packet_iter{video_packet_collections_.find(video_sender_packet_data.frame_id)};
        if (video_packet_iter == video_packet_collections_.end()) {
            std::tie(video_packet_iter, std::ignore) = video_packet_collections_.insert({video_sender_packet_data.frame_id,
                                                                                         std::vector<std::optional<VideoSenderPacket>>(video_sender_packet_data.packet_count)});
            // Assign the largest new frame_id.
            if(!added_frame_id || added_frame_id < video_sender_packet_data.frame_id)
                added_frame_id = video_sender_packet_data.frame_id;
        }

        video_packet_iter->second[video_sender_packet_data.packet_index] = std::move(video_sender_packet_data);
        //std::cout << "video packet " << video_sender_packet_data.frame_id << ":" << video_sender_packet_data.packet_index << "\n";
    }

    // Collect the received parity packets.
    for (auto& parity_sender_packet_data : parity_packets) {
        if (parity_sender_packet_data.frame_id <= last_frame_id)
            continue;

        auto parity_packet_iter{parity_packet_collections_.find(parity_sender_packet_data.frame_id)};
        if (parity_packet_iter == parity_packet_collections_.end())
            std::tie(parity_packet_iter, std::ignore) = parity_packet_collections_.insert({parity_sender_packet_data.frame_id,
                                                                                           std::vector<std::optional<ParitySenderPacket>>(parity_sender_packet_data.packet_count)});

        parity_packet_iter->second[parity_sender_packet_data.packet_index] = std::move(parity_sender_packet_data);
        //std::cout << "parity packet " << parity_sender_packet_data.frame_id << ":" << parity_sender_packet_data.packet_index << "\n";
    }

    // Try FEC if a new frame was received.
    if (added_frame_id) {
        //std::cout << "ADDED FRAME ID: " << *added_frame_id << " (renderer frame_id: " << video_renderer_state.frame_id << ")\n";
        for (auto& video_packet_collection : video_packet_collections_) {
            const int frame_id{video_packet_collection.first};
            std::vector<std::optional<VideoSenderPacket>>* video_packets_ptr{&video_packet_collection.second};
            //std::cout << "  fec frame_id: " << frame_id << "\n";
            // Skip the frame that just got added or even newer.
            if (frame_id >= added_frame_id)
                continue;

            // Find the parity packet collection corresponding to the video packet collection.
            auto parity_packet_collections_ref{parity_packet_collections_.find(frame_id)};
            // Skip if there is no parity packet collection for the video frame.
            if (parity_packet_collections_ref == parity_packet_collections_.end()) {
                //std::cout << "  no parity packet collection\n";
                continue;
            }
            std::vector<std::optional<ParitySenderPacket>>* parity_packets_ptr{&parity_packet_collections_ref->second};

            // Loop per each parity packet.
            // Collect video packet indices to request.
            std::vector<int> video_packet_indiecs_to_request;
            std::vector<int> parity_packet_indiecs_to_request;
            int fec_count{0};
            //for (auto& parity_packet : parity_packet_collections_ref->second) {
            for (gsl::index parity_packet_index{0}; parity_packet_index < parity_packets_ptr->size(); ++parity_packet_index) {
                // Range of the video packets that correspond to the parity packet.
                gsl::index video_packet_start_index{parity_packet_index * KH_FEC_PARITY_GROUP_SIZE};
                // Pick the end index with the end of video packet indices in mind (i.e., prevent overflow).
                gsl::index video_packet_end_index{std::min<int>(video_packet_start_index + KH_FEC_PARITY_GROUP_SIZE,
                                                                video_packets_ptr->size())};

                // If the parity packet is missing, request all missing video packets and skip the FEC process.
                // Also request the parity packet if there is a relevant missing video packet.
                if (!parity_packets_ptr->at(parity_packet_index)) {
                    bool parity_packet_needed{false};
                    for (gsl::index video_packet_index{video_packet_start_index}; video_packet_index < video_packet_end_index; ++video_packet_index) {
                        if (!video_packets_ptr->at(video_packet_index)) {
                            video_packet_indiecs_to_request.push_back(video_packet_index);
                            parity_packet_needed = true;
                        }
                    }
                    if (parity_packet_needed)
                        parity_packet_indiecs_to_request.push_back(parity_packet_index);
                    continue;
                }

                // Find if there is existing video packets and missing video packet indices.
                // Check all video packets that relates to this parity packet.
                std::vector<VideoSenderPacket*> existing_video_packet_ptrs;
                std::vector<int> missing_video_packet_indices;
                for (gsl::index video_packet_index{video_packet_start_index}; video_packet_index < video_packet_end_index; ++video_packet_index) {
                    if (video_packets_ptr->at(video_packet_index)) {
                        existing_video_packet_ptrs.push_back(&*video_packets_ptr->at(video_packet_index));
                    } else {
                        missing_video_packet_indices.push_back(video_packet_index);
                    }
                }

                // Skip if there all video packets already exist.
                if (missing_video_packet_indices.size() == 0)
                    continue;

                // XOR based FEC only works for a single missing packet.
                if (missing_video_packet_indices.size() > 1) {
                    for (int missing_video_packet_index : missing_video_packet_indices) {
                        // Add the missing video packet indices for the vector to request them.
                        video_packet_indiecs_to_request.push_back(missing_video_packet_index);
                    }
                    continue;
                }

                ++fec_count;
                // The missing video packet index.
                const int missing_video_packet_index{missing_video_packet_indices[0]};

                // Reconstruct the missing video packet.
                // TODO: Fix sesion_id and type of this.
                VideoSenderPacket fec_video_packet;
                fec_video_packet.sender_id = 0;
                fec_video_packet.type = SenderPacketType::Video;
                fec_video_packet.frame_id = frame_id;
                fec_video_packet.packet_index = missing_video_packet_index;
                fec_video_packet.packet_count = video_packets_ptr->size();
                // Assign from the parity packet here since other video packets will be XOR'ed in the below loop.
                fec_video_packet.message_data = parity_packets_ptr->at(parity_packet_index)->bytes;

                for (auto existing_video_packet_ptr : existing_video_packet_ptrs) {
                    for (gsl::index i{0}; i < fec_video_packet.message_data.size(); ++i)
                        fec_video_packet.message_data[i] ^= existing_video_packet_ptr->message_data[i];
                }

                // Insert the reconstructed packet.
                video_packet_collections_.at(frame_id)[missing_video_packet_index] = std::move(fec_video_packet);
            }
            // Request the video packets that FEC was not enough to fix.
            udp_socket.send(create_request_receiver_packet(receiver_id_, frame_id, video_packet_indiecs_to_request, parity_packet_indiecs_to_request).bytes, remote_endpoint_);
            //std::cout << "  video_packet_indiecs_to_request.size(): " << video_packet_indiecs_to_request.size() << "\n"
            //          << "  parity_packet_indiecs_to_request.size(): " << parity_packet_indiecs_to_request.size() << "\n"
            //          << "  fec_count: " << fec_count << "\n";

            //for (int i = 0; i < video_packet_collection.second.size(); ++i) {
            //    if (!video_packet_collection.second[i]) {
            //        std::cout << "  video packet index " << i << " is still missing\n";
            //    }
            //}

            //for (int i = 0; i < parity_packet_collections_ref->second.size(); ++i) {
            //    if (!parity_packet_collections_ref->second[i]) {
            //        std::cout << "  parity packet index " << i << " is missing\n";
            //    }
            //}
        }
    }

    // Find all full collections and extract messages from them.
    for (auto it = video_packet_collections_.begin(); it != video_packet_collections_.end();) {
        bool full = true;
        for (auto& video_sender_packet_data : it->second) {
            if (!video_sender_packet_data) {
                full = false;
                break;
            }
        }

        if (full) {
            std::vector<VideoSenderPacket*> video_packet_ptrs(it->second.size());
            for (gsl::index i{0}; i < video_packet_ptrs.size(); ++i)
                video_packet_ptrs[i] = &*(it->second[i]);

            video_messages.insert({it->first, read_video_sender_message(merge_video_sender_packets(video_packet_ptrs))});
            it = video_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up frame_packet_collections.
    for (auto it = video_packet_collections_.begin(); it != video_packet_collections_.end();) {
        if (it->first <= last_frame_id) {
            it = video_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up parity_packet_collections.
    for (auto it = parity_packet_collections_.begin(); it != parity_packet_collections_.end();) {
        if (it->first <= last_frame_id) {
            it = parity_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }
}
}