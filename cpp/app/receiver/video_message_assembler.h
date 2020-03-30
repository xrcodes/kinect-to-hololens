#pragma once

namespace kh
{

class VideoMessageAssembler
{
public:
    VideoMessageAssembler(const int session_id, const asio::ip::udp::endpoint remote_endpoint)
        : session_id_{session_id}, remote_endpoint_{remote_endpoint}, video_packet_collections_{}, fec_packet_collections_{}, video_message_queue_{}
    {
    }

    void reassemble(UdpSocket& udp_socket,
                    moodycamel::ReaderWriterQueue<VideoSenderPacketData>& video_packet_data_queue,
                    moodycamel::ReaderWriterQueue<FecSenderPacketData>& fec_packet_data_queue,
                    int last_video_frame_id)
    {
        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        FecSenderPacketData fec_sender_packet_data;
        while (fec_packet_data_queue.try_dequeue(fec_sender_packet_data)) {
            if (fec_sender_packet_data.frame_id <= last_video_frame_id)
                continue;

            auto fec_packet_iter = fec_packet_collections_.find(fec_sender_packet_data.frame_id);
            if (fec_packet_iter == fec_packet_collections_.end())
                std::tie(fec_packet_iter, std::ignore) = fec_packet_collections_.insert({fec_sender_packet_data.frame_id,
                                                                                        std::vector<std::optional<FecSenderPacketData>>(fec_sender_packet_data.packet_count)});

            fec_packet_iter->second[fec_sender_packet_data.packet_index] = std::move(fec_sender_packet_data);
        }

        VideoSenderPacketData video_sender_packet_data;
        while (video_packet_data_queue.try_dequeue(video_sender_packet_data)) {
            if (video_sender_packet_data.frame_id <= last_video_frame_id)
                continue;

            // If there is a packet for a new frame, check the previous frames, and if
            // there is a frame with missing packets, try to create them using xor packets.
            // If using the xor packets fails, request the sender to retransmit the packets.
            if (video_packet_collections_.find(video_sender_packet_data.frame_id) == video_packet_collections_.end()) {
                video_packet_collections_.insert({video_sender_packet_data.frame_id,
                                                 std::vector<std::optional<VideoSenderPacketData>>(video_sender_packet_data.packet_count)});

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                for (auto& collection_pair : video_packet_collections_) {
                    if (collection_pair.first < video_sender_packet_data.frame_id) {
                        const int missing_frame_id{collection_pair.first};
                        std::vector<int> missing_packet_indices;
                        for (int i = 0; i < collection_pair.second.size(); ++i) {
                            if (!collection_pair.second[i])
                                missing_packet_indices.push_back(i);
                        }

                        // Try correction using XOR FEC packets.
                        std::vector<int> fec_failed_packet_indices;
                        std::vector<int> fec_packet_indices;

                        // missing_packet_index cannot get error corrected if there is another missing_packet_index
                        // that belongs to the same XOR FEC packet...
                        for (int i : missing_packet_indices) {
                            bool found{false};
                            for (int j : missing_packet_indices) {
                                if (i == j)
                                    continue;

                                if ((i / FEC_GROUP_SIZE) == (j / FEC_GROUP_SIZE)) {
                                    found = true;
                                    break;
                                }
                            }
                            if (found) {
                                fec_failed_packet_indices.push_back(i);
                            } else {
                                fec_packet_indices.push_back(i);
                            }
                        }

                        for (int fec_packet_index : fec_packet_indices) {
                            // Try getting the XOR FEC packet for correction.
                            const int xor_packet_index{fec_packet_index / FEC_GROUP_SIZE};

                            if (fec_packet_collections_.find(missing_frame_id) == fec_packet_collections_.end()) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            const auto fec_packet_data{fec_packet_collections_.at(missing_frame_id)[xor_packet_index]};
                            // Give up if there is no xor packet yet.
                            if (!fec_packet_data) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            const auto fec_start{TimePoint::now()};

                            VideoSenderPacketData fec_video_packet_data;
                            fec_video_packet_data.frame_id = missing_frame_id;
                            fec_video_packet_data.packet_index = video_sender_packet_data.packet_index;
                            fec_video_packet_data.packet_count = video_sender_packet_data.packet_count;
                            fec_video_packet_data.message_data = fec_packet_data->bytes;

                            const int begin_frame_packet_index{xor_packet_index * FEC_GROUP_SIZE};
                            const int end_frame_packet_index{std::min<int>(begin_frame_packet_index + FEC_GROUP_SIZE,
                                                                           collection_pair.second.size())};
                            // Run bitwise XOR with all other packets belonging to the same XOR FEC packet.
                            for (gsl::index i = begin_frame_packet_index; i < end_frame_packet_index; ++i) {
                                if (i == fec_packet_index)
                                    continue;

                                for (gsl::index j{0}; j < fec_video_packet_data.message_data.size(); ++j)
                                    fec_video_packet_data.message_data[j] ^= collection_pair.second[i]->message_data[j];
                            }

                            const auto fec_time{TimePoint::now() - fec_start};

                            //printf("restored %d %d %lf\n", missing_frame_id, fec_packet_index, fec_time.count() / 1000000.0f);
                            video_packet_collections_.at(missing_frame_id)[fec_packet_index] = std::move(fec_video_packet_data);
                        } // end of for (int missing_packet_index : missing_packet_indices)

                        //for (int fec_failed_packet_index : fec_failed_packet_indices) {
                        //    printf("request %d %d\n", missing_frame_id, fec_failed_packet_index);
                        //}

                        udp_socket.send(create_request_receiver_packet_bytes(session_id_, missing_frame_id, fec_failed_packet_indices), remote_endpoint_);
                    }
                }
                /////////////////////////////////
                // Forward Error Correction End//
                /////////////////////////////////
            }
            // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
            // which was for reacting to a packet for a new frame.

            video_packet_collections_.at(video_sender_packet_data.frame_id)[video_sender_packet_data.packet_index] = std::move(video_sender_packet_data);
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
                std::vector<gsl::span<std::byte>> video_sender_message_data_set(it->second.size());
                for (gsl::index i{0}; i < video_sender_message_data_set.size(); ++i)
                    video_sender_message_data_set[i] = gsl::span<std::byte>{it->second[i]->message_data};

                video_message_queue_.enqueue({it->first, parse_video_sender_message_bytes(merge_video_sender_message_bytes(video_sender_message_data_set))});
                it = video_packet_collections_.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up frame_packet_collections.
        for (auto it = video_packet_collections_.begin(); it != video_packet_collections_.end();) {
            if (it->first <= last_video_frame_id) {
                it = video_packet_collections_.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up xor_packet_collections.
        for (auto it = fec_packet_collections_.begin(); it != fec_packet_collections_.end();) {
            if (it->first <= last_video_frame_id) {
                it = fec_packet_collections_.erase(it);
            } else {
                ++it;
            }
        }
    }

    moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>>& video_message_queue() { return video_message_queue_; }

private:
    static constexpr int FEC_GROUP_SIZE{5};
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;
    std::unordered_map<int, std::vector<std::optional<VideoSenderPacketData>>> video_packet_collections_;
    std::unordered_map<int, std::vector<std::optional<FecSenderPacketData>>> fec_packet_collections_;
    moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>> video_message_queue_;
};
}