#include "video_message_assembler.h"

#include <iostream>

namespace kh
{
VideoMessageAssembler::VideoMessageAssembler(const int session_id, const asio::ip::udp::endpoint remote_endpoint)
    : session_id_{session_id}, remote_endpoint_{remote_endpoint}, video_packet_collections_{}, parity_packet_collections_{}
{
}

void VideoMessageAssembler::assemble(UdpSocket& udp_socket,
                std::vector<VideoSenderPacketData>& video_packet_data_vector,
                std::vector<ParitySenderPacketData>& parity_packet_data_vector,
                VideoRendererState video_renderer_state,
                moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>>& video_message_queue)
{
    std::optional<int> added_frame_id{std::nullopt};
    // Collect the received video packets.
    for (auto& video_sender_packet_data : video_packet_data_vector) {
        if (video_sender_packet_data.frame_id <= video_renderer_state.frame_id)
            continue;

        auto video_packet_iter{video_packet_collections_.find(video_sender_packet_data.frame_id)};
        if (video_packet_iter == video_packet_collections_.end()) {
            std::tie(video_packet_iter, std::ignore) = video_packet_collections_.insert({video_sender_packet_data.frame_id,
                                                                                         std::vector<std::optional<VideoSenderPacketData>>(video_sender_packet_data.packet_count)});
            added_frame_id = video_sender_packet_data.frame_id;
        }

        video_packet_iter->second[video_sender_packet_data.packet_index] = std::move(video_sender_packet_data);
        std::cout << "video packet " << video_sender_packet_data.frame_id << ":" << video_sender_packet_data.packet_index << "\n";
    }

    // Collect the received parity packets.
    for (auto& parity_sender_packet_data : parity_packet_data_vector) {
        if (parity_sender_packet_data.frame_id <= video_renderer_state.frame_id)
            continue;

        auto parity_packet_iter{parity_packet_collections_.find(parity_sender_packet_data.frame_id)};
        if (parity_packet_iter == parity_packet_collections_.end())
            std::tie(parity_packet_iter, std::ignore) = parity_packet_collections_.insert({parity_sender_packet_data.frame_id,
                                                                                            std::vector<std::optional<ParitySenderPacketData>>(parity_sender_packet_data.packet_count)});

        parity_packet_iter->second[parity_sender_packet_data.packet_index] = std::move(parity_sender_packet_data);
        std::cout << "parity packet " << parity_sender_packet_data.frame_id << ":" << parity_sender_packet_data.packet_index << "\n";
    }

    // Try FEC if a new frame was received.
    if (added_frame_id) {
        std::cout << "ADDED FRAME ID: " << *added_frame_id << "\n";
        for (auto& video_packet_collection : video_packet_collections_) {
            const int frame_id{video_packet_collection.first};
            std::cout << "fec frame_id: " << frame_id << "\n";
            // Skip the frame that just got added or even newer.
            if (frame_id >= added_frame_id)
                continue;

            // Find the parity packet collection corresponding to the video packet collection.
            auto parity_packet_collections_ref{parity_packet_collections_.find(frame_id)};
            // Skip if there is no parity packet collection for the video frame.
            if (parity_packet_collections_ref == parity_packet_collections_.end())
                continue;

            // Loop per each parity packet.
            // Collect video packet indices to request.
            std::vector<int> video_packet_indiecs_to_request;
            std::vector<int> parity_packet_indiecs_to_request;
            int fec_count{0};
            //for (auto& parity_packet : parity_packet_collections_ref->second) {
            for (gsl::index parity_packet_index{0}; parity_packet_index < parity_packet_collections_ref->second.size(); ++parity_packet_index) {
                // Range of the video packets that correspond to the parity packet.
                gsl::index video_packet_start_index{parity_packet_index * FEC_GROUP_SIZE};
                // Pick the end index with the end of video packet indices in mind (i.e., prevent overflow).
                gsl::index video_packet_end_index{std::min<int>(video_packet_start_index + FEC_GROUP_SIZE,
                                                                video_packet_collection.second.size())};

                // If the parity packet is missing, request all missing video packets and skip the FEC process.
                if (!parity_packet_collections_ref->second[parity_packet_index]) {
                    std::cout << "  parity_packet " << parity_packet_index << " is empty\n";
                    parity_packet_indiecs_to_request.push_back(parity_packet_index);
                    for (gsl::index video_packet_index{video_packet_start_index}; video_packet_index < video_packet_end_index; ++video_packet_index) {
                        if (!video_packet_collection.second[video_packet_index]) {
                            std::cout << "  directly request " << video_packet_index << "\n";
                            video_packet_indiecs_to_request.push_back(video_packet_index);
                        }
                    }
                    continue;
                }

                // Find if there is existing video packets and missing video packet indices.
                // Check all video packets that relates to this parity packet.
                std::vector<VideoSenderPacketData*> existing_video_packet_ptrs;
                std::vector<int> missing_video_packet_indices;
                for (gsl::index video_packet_index{video_packet_start_index}; video_packet_index < video_packet_end_index; ++video_packet_index) {
                    if (video_packet_collection.second[video_packet_index]) {
                        existing_video_packet_ptrs.push_back(&*video_packet_collection.second[video_packet_index]);
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
                VideoSenderPacketData fec_video_packet_data;
                fec_video_packet_data.frame_id = frame_id;
                fec_video_packet_data.packet_index = missing_video_packet_index;
                fec_video_packet_data.packet_count = video_packet_collection.second.size();
                // Assign from the parity packet here since other video packets will be XOR'ed in the below loop.
                fec_video_packet_data.message_data = parity_packet_collections_ref->second[parity_packet_index]->bytes;

                for (auto existing_video_packet_ptr : existing_video_packet_ptrs) {
                    for (gsl::index i{0}; i < fec_video_packet_data.message_data.size(); ++i)
                        fec_video_packet_data.message_data[i] ^= existing_video_packet_ptr->message_data[i];
                }

                // Insert the reconstructed packet.
                video_packet_collections_.at(frame_id)[missing_video_packet_index] = std::move(fec_video_packet_data);
            }
            // Request the video packets that FEC was not enough to fix.
            udp_socket.send(create_request_receiver_packet_bytes(session_id_, frame_id, video_packet_indiecs_to_request, parity_packet_indiecs_to_request), remote_endpoint_);
            std::cout << "  video_packet_indiecs_to_request.size(): " << video_packet_indiecs_to_request.size() << "\n"
                      << "  fec_count: " << fec_count << "\n";

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





    //for (auto& video_sender_packet_data : video_packet_data_vector) {
    //    if (video_sender_packet_data.frame_id <= video_renderer_state.frame_id)
    //        continue;

    //    // If there is a packet for a new frame, check the previous frames, and if
    //    // there is a frame with missing packets, try to create them using xor packets.
    //    // If using the xor packets fails, request the sender to retransmit the packets.
    //    if (video_packet_collections_.find(video_sender_packet_data.frame_id) == video_packet_collections_.end()) {
    //        video_packet_collections_.insert({video_sender_packet_data.frame_id,
    //                                            std::vector<std::optional<VideoSenderPacketData>>(video_sender_packet_data.packet_count)});

    //        ///////////////////////////////////
    //        // Forward Error Correction Start//
    //        ///////////////////////////////////
    //        // Request missing packets of the previous frames.
    //        for (auto& collection_pair : video_packet_collections_) {
    //            if (collection_pair.first < video_sender_packet_data.frame_id) {
    //                const int missing_frame_id{collection_pair.first};
    //                std::vector<int> missing_packet_indices;
    //                for (int i = 0; i < collection_pair.second.size(); ++i) {
    //                    if (!collection_pair.second[i])
    //                        missing_packet_indices.push_back(i);
    //                }

    //                // Try correction using parity packets.
    //                std::vector<int> fec_failed_packet_indices;
    //                std::vector<int> fec_packet_indices;

    //                // missing_packet_index cannot get error corrected if there is another missing_packet_index
    //                // that belongs to the same XOR FEC packet...
    //                for (int i : missing_packet_indices) {
    //                    bool found{false};
    //                    for (int j : missing_packet_indices) {
    //                        if (i == j)
    //                            continue;

    //                        if ((i / FEC_GROUP_SIZE) == (j / FEC_GROUP_SIZE)) {
    //                            found = true;
    //                            break;
    //                        }
    //                    }
    //                    if (found) {
    //                        fec_failed_packet_indices.push_back(i);
    //                    } else {
    //                        fec_packet_indices.push_back(i);
    //                    }
    //                }

    //                for (int fec_packet_index : fec_packet_indices) {
    //                    // Try getting the XOR FEC packet for correction.
    //                    const int xor_packet_index{fec_packet_index / FEC_GROUP_SIZE};

    //                    if (parity_packet_collections_.find(missing_frame_id) == parity_packet_collections_.end()) {
    //                        fec_failed_packet_indices.push_back(fec_packet_index);
    //                        continue;
    //                    }

    //                    const auto fec_packet_data{parity_packet_collections_.at(missing_frame_id)[xor_packet_index]};
    //                    // Give up if there is no xor packet yet.
    //                    if (!fec_packet_data) {
    //                        fec_failed_packet_indices.push_back(fec_packet_index);
    //                        continue;
    //                    }

    //                    const auto fec_start{TimePoint::now()};

    //                    VideoSenderPacketData fec_video_packet_data;
    //                    fec_video_packet_data.frame_id = missing_frame_id;
    //                    fec_video_packet_data.packet_index = video_sender_packet_data.packet_index;
    //                    fec_video_packet_data.packet_count = video_sender_packet_data.packet_count;
    //                    fec_video_packet_data.message_data = fec_packet_data->bytes;

    //                    const int begin_frame_packet_index{xor_packet_index * FEC_GROUP_SIZE};
    //                    const int end_frame_packet_index{std::min<int>(begin_frame_packet_index + FEC_GROUP_SIZE,
    //                                                                    collection_pair.second.size())};
    //                    // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
    //                    for (gsl::index i = begin_frame_packet_index; i < end_frame_packet_index; ++i) {
    //                        if (i == fec_packet_index)
    //                            continue;

    //                        for (gsl::index j{0}; j < fec_video_packet_data.message_data.size(); ++j)
    //                            fec_video_packet_data.message_data[j] ^= collection_pair.second[i]->message_data[j];
    //                    }

    //                    const auto fec_time{TimePoint::now() - fec_start};

    //                    //printf("restored %d %d %lf\n", missing_frame_id, fec_packet_index, fec_time.count() / 1000000.0f);
    //                    video_packet_collections_.at(missing_frame_id)[fec_packet_index] = std::move(fec_video_packet_data);
    //                } // end of for (int missing_packet_index : missing_packet_indices)

    //                //for (int fec_failed_packet_index : fec_failed_packet_indices) {
    //                //    printf("request %d %d\n", missing_frame_id, fec_failed_packet_index);
    //                //}

    //                udp_socket.send(create_request_receiver_packet_bytes(session_id_, missing_frame_id, fec_failed_packet_indices), remote_endpoint_);
    //            }
    //        }
    //        /////////////////////////////////
    //        // Forward Error Correction End//
    //        /////////////////////////////////
    //    }
    //    // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
    //    // which was for reacting to a packet for a new frame.

    //    video_packet_collections_.at(video_sender_packet_data.frame_id)[video_sender_packet_data.packet_index] = std::move(video_sender_packet_data);
    //}

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
            std::vector<gsl::span<std::byte>> video_sender_message_data_set(it->second.size());
            for (gsl::index i{0}; i < video_sender_message_data_set.size(); ++i)
                video_sender_message_data_set[i] = gsl::span<std::byte>{it->second[i]->message_data};

            video_message_queue.enqueue({it->first, parse_video_sender_message_bytes(merge_video_sender_message_bytes(video_sender_message_data_set))});
            it = video_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up frame_packet_collections.
    for (auto it = video_packet_collections_.begin(); it != video_packet_collections_.end();) {
        if (it->first <= video_renderer_state.frame_id) {
            it = video_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }

    // Clean up xor_packet_collections.
    for (auto it = parity_packet_collections_.begin(); it != parity_packet_collections_.end();) {
        if (it->first <= video_renderer_state.frame_id) {
            it = parity_packet_collections_.erase(it);
        } else {
            ++it;
        }
    }
}
}