#include <algorithm>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>
#include <gsl/gsl>
#include <readerwriterqueue/readerwriterqueue.h>
#include "helper/opencv_helper.h"
#include "kh_vp8.h"
#include "kh_trvl.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "native/kh_time.h"
#include "helper/soundio_helper.h"
#include "kh_opus.h"

namespace kh
{
void receive_sender_packets(int sender_id,
                            bool& stopped,
                            UdpSocket& udp_socket,
                            moodycamel::ReaderWriterQueue<VideoSenderPacketData>& video_packet_data_queue,
                            moodycamel::ReaderWriterQueue<FecSenderPacketData>& fec_packet_data_queue,
                            moodycamel::ReaderWriterQueue<AudioSenderPacketData>& audio_packet_data_queue)
{
    while (!stopped) {
        while (auto packet_bytes{udp_socket.receive()}) {
            const int session_id{get_session_id_from_sender_packet_bytes(*packet_bytes)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(*packet_bytes)};

            if (session_id != sender_id)
                continue;

            if (packet_type == SenderPacketType::Video) {
                video_packet_data_queue.enqueue(parse_video_sender_packet_bytes(*packet_bytes));
            } else if (packet_type == SenderPacketType::Fec) {
                fec_packet_data_queue.enqueue(parse_fec_sender_packet_bytes(*packet_bytes));
            } else if (packet_type == SenderPacketType::Audio) {
                audio_packet_data_queue.enqueue(parse_audio_sender_packet_bytes(*packet_bytes));
            }
        }
    }

    stopped = true;
}

void reassemble_video_messages(bool& stopped,
                               UdpSocket& udp_socket,
                               moodycamel::ReaderWriterQueue<VideoSenderPacketData>& video_packet_data_queue,
                               moodycamel::ReaderWriterQueue<FecSenderPacketData>& fec_packet_data_queue,
                               moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>>& video_message_queue,
                               int& last_video_frame_id)
{
    constexpr int FEC_MAX_GROUP_SIZE{5};

    std::unordered_map<int, std::vector<std::optional<VideoSenderPacketData>>> video_packet_collections;
    std::unordered_map<int, std::vector<std::optional<FecSenderPacketData>>> fec_packet_collections;
    while (!stopped) {
        // The logic for XOR FEC packets are almost the same to frame packets.
        // The operations for XOR FEC packets should happen before the frame packets
        // so that frame packet can be created with XOR FEC packets when a missing
        // frame packet is detected.
        FecSenderPacketData fec_sender_packet_data;
        while (fec_packet_data_queue.try_dequeue(fec_sender_packet_data)) {
            if (fec_sender_packet_data.frame_id <= last_video_frame_id)
                continue;

            auto fec_packet_iter = fec_packet_collections.find(fec_sender_packet_data.frame_id);
            if (fec_packet_iter == fec_packet_collections.end())
                std::tie(fec_packet_iter, std::ignore) = fec_packet_collections.insert({fec_sender_packet_data.frame_id,
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
            if (video_packet_collections.find(video_sender_packet_data.frame_id) == video_packet_collections.end()) {
                video_packet_collections.insert({video_sender_packet_data.frame_id,
                                                 std::vector<std::optional<VideoSenderPacketData>>(video_sender_packet_data.packet_count)});

                ///////////////////////////////////
                // Forward Error Correction Start//
                ///////////////////////////////////
                // Request missing packets of the previous frames.
                for (auto& collection_pair : video_packet_collections) {
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

                                if ((i / FEC_MAX_GROUP_SIZE) == (j / FEC_MAX_GROUP_SIZE)) {
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
                            const int xor_packet_index{fec_packet_index / FEC_MAX_GROUP_SIZE};

                            if (fec_packet_collections.find(missing_frame_id) == fec_packet_collections.end()) {
                                fec_failed_packet_indices.push_back(fec_packet_index);
                                continue;
                            }

                            const auto fec_packet_data{fec_packet_collections.at(missing_frame_id)[xor_packet_index]};
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

                            const int begin_frame_packet_index{xor_packet_index * FEC_MAX_GROUP_SIZE};
                            const int end_frame_packet_index{std::min<int>(begin_frame_packet_index + FEC_MAX_GROUP_SIZE,
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
                            video_packet_collections.at(missing_frame_id)[fec_packet_index] = std::move(fec_video_packet_data);
                        } // end of for (int missing_packet_index : missing_packet_indices)

                        for (int fec_failed_packet_index : fec_failed_packet_indices) {
                            printf("request %d %d\n", missing_frame_id, fec_failed_packet_index);
                        }
                        
                        udp_socket.send(create_request_receiver_packet_bytes(missing_frame_id, fec_failed_packet_indices));
                    }
                }
                /////////////////////////////////
                // Forward Error Correction End//
                /////////////////////////////////
            }
            // End of if (frame_packet_collections.find(frame_id) == frame_packet_collections.end())
            // which was for reacting to a packet for a new frame.

            video_packet_collections.at(video_sender_packet_data.frame_id)[video_sender_packet_data.packet_index] = std::move(video_sender_packet_data);
        }

        // Find all full collections and extract messages from them.
        for (auto it = video_packet_collections.begin(); it != video_packet_collections.end();) {
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
                it = video_packet_collections.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up frame_packet_collections.
        for (auto it = video_packet_collections.begin(); it != video_packet_collections.end();) {
            if (it->first <= last_video_frame_id) {
                it = video_packet_collections.erase(it);
            } else {
                ++it;
            }
        }

        // Clean up xor_packet_collections.
        for (auto it = fec_packet_collections.begin(); it != fec_packet_collections.end();) {
            if (it->first <= last_video_frame_id) {
                it = fec_packet_collections.erase(it);
            } else {
                ++it;
            }
        }
    }

    stopped = true;
}

void consume_audio_packets(bool& stopped,
                           moodycamel::ReaderWriterQueue<AudioSenderPacketData>& audio_packet_data_queue)
{
    Audio audio;
    auto default_speaker{audio.getDefaultOutputDevice()};
    AudioOutStream default_speaker_stream(default_speaker);
    // These settings are those generic and similar to Azure Kinect's.
    // It is set to be Stereo, which is the default setting of Unity3D.
    default_speaker_stream.get()->format = SoundIoFormatFloat32LE;
    default_speaker_stream.get()->sample_rate = KH_SAMPLE_RATE;
    default_speaker_stream.get()->layout = *soundio_channel_layout_get_builtin(SoundIoChannelLayoutIdStereo);
    default_speaker_stream.get()->software_latency = KH_LATENCY_SECONDS;
    default_speaker_stream.get()->write_callback = soundio_callback::write_callback;
    default_speaker_stream.get()->underflow_callback = soundio_callback::underflow_callback;
    default_speaker_stream.open();

    const int default_speaker_bytes_per_second{default_speaker_stream.get()->sample_rate * default_speaker_stream.get()->bytes_per_frame};
    assert(KH_BYTES_PER_SECOND == default_speaker_bytes_per_second);

    constexpr int capacity{gsl::narrow_cast<int>(KH_LATENCY_SECONDS * 2 * KH_BYTES_PER_SECOND)};

    soundio_callback::ring_buffer = soundio_ring_buffer_create(audio.get(), capacity);
    if (!soundio_callback::ring_buffer)
        throw std::exception("Failed in soundio_ring_buffer_create()...");

    AudioDecoder audio_decoder{KH_SAMPLE_RATE, KH_CHANNEL_COUNT};

    default_speaker_stream.start();

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm;

    std::vector<AudioSenderPacketData> audio_packet_data_set;
    int last_audio_frame_id{-1};
    while (!stopped) {
        soundio_flush_events(audio.get());

        AudioSenderPacketData audio_sender_packet_data;
        while (audio_packet_data_queue.try_dequeue(audio_sender_packet_data))
            audio_packet_data_set.push_back(audio_sender_packet_data);

        std::sort(audio_packet_data_set.begin(),
                  audio_packet_data_set.end(),
                  [](AudioSenderPacketData& a, AudioSenderPacketData& b) { return a.frame_id < b.frame_id; });

        char* write_ptr = soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer);
        int free_bytes = soundio_ring_buffer_free_count(soundio_callback::ring_buffer);

        constexpr int FRAME_BYTE_SIZE{sizeof(float) * pcm.size()};

        int write_cursor = 0;
        auto packet_it = audio_packet_data_set.begin();
        while ((free_bytes - write_cursor) >= FRAME_BYTE_SIZE) {
            if (packet_it == audio_packet_data_set.end())
                break;

            int frame_size;
            if (packet_it->frame_id <= last_audio_frame_id) {
                // If a packet is about the past, throw it away and try again.
                packet_it = audio_packet_data_set.erase(packet_it);
                continue;
            } else if (packet_it->frame_id == last_audio_frame_id + 1) {
                // When the packet for the next audio frame is found,
                // use it and erase it.
                frame_size = audio_decoder.decode(packet_it->opus_frame, pcm.data(), KH_SAMPLES_PER_FRAME, 0);
                packet_it = audio_packet_data_set.erase(packet_it);
            } else {
                // If not, let opus know there is a packet loss.
                frame_size = audio_decoder.decode(std::nullopt, pcm.data(), KH_SAMPLES_PER_FRAME, 0);
            }

            if (frame_size < 0) {
                throw std::runtime_error(std::string("Failed to decode audio: ") + opus_strerror(frame_size));
            }

            memcpy(write_ptr + write_cursor, pcm.data(), FRAME_BYTE_SIZE);

            ++last_audio_frame_id;
            write_cursor += FRAME_BYTE_SIZE;
        }

        soundio_ring_buffer_advance_write_ptr(soundio_callback::ring_buffer, write_cursor);
    }
}

void consume_video_message(bool& stopped,
                           int depth_width,
                           int depth_height,
                           UdpSocket& udp_socket,
                           moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>>& video_message_queue,
                           int& last_video_frame_id)
{
    Vp8Decoder color_decoder;
    TrvlDecoder depth_decoder{depth_width * depth_height};

    std::map<int, VideoSenderMessageData> frame_messages;
    auto frame_start{TimePoint::now()};
    while (!stopped) {
        std::pair<int, VideoSenderMessageData> frame_message;
        while (video_message_queue.try_dequeue(frame_message)) {
            frame_messages.insert(std::move(frame_message));
        }

        if (frame_messages.empty())
            continue;

        std::optional<int> begin_frame_id;
        // If there is a key frame, use the most recent one.
        for (auto& frame_message_pair : frame_messages) {
            if (frame_message_pair.first <= last_video_frame_id)
                continue;

            if (frame_message_pair.second.keyframe)
                begin_frame_id = frame_message_pair.first;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!begin_frame_id) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            if (frame_messages.find(last_video_frame_id + 1) != frame_messages.end()) {
                begin_frame_id = last_video_frame_id + 1;
            } else {
                // Wait for more frames if there is way to render without glitches.
                continue;
            }
        }

        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        const auto decoder_start{TimePoint::now()};
        for (int i = *begin_frame_id; ; ++i) {
            // break loop is there is no frame with frame_id i.
            if (frame_messages.find(i) == frame_messages.end())
                break;

            const auto frame_message_pair_ptr{&frame_messages[i]};

            last_video_frame_id = i;

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder.decode(frame_message_pair_ptr->color_encoder_frame);
            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder.decode(frame_message_pair_ptr->depth_encoder_frame, frame_message_pair_ptr->keyframe);
        }
        const auto decoder_time{TimePoint::now() - decoder_start};
        const auto frame_time{TimePoint::now() - frame_start};
        frame_start = TimePoint::now();

        udp_socket.send(create_report_receiver_packet_bytes(last_video_frame_id,
                                                            decoder_time.ms(),
                                                            frame_time.ms()));

        auto color_mat{createCvMatFromYuvImage(createYuvImageFromAvFrame(*ffmpeg_frame->av_frame()))};
        auto depth_mat{createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()),
                                                       depth_width,
                                                       depth_height)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            break;

        // Remove frame messages before the rendered frame.
        for (auto it = frame_messages.begin(); it != frame_messages.end();) {
            if (it->first < last_video_frame_id) {
                it = frame_messages.erase(it);
            } else {
                ++it;
            }
        }
    }

    stopped = true;
}

void receive_frames(std::string ip_address, int port)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;
    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});
    UdpSocket udp_socket{std::move(socket), asio::ip::udp::endpoint{asio::ip::address::from_string(ip_address), gsl::narrow_cast<unsigned short>(port)}};

    int sender_session_id;
    int depth_width;
    int depth_height;
    // When ping then check if a init packet arrived.
    // Repeat until it happens.
    int ping_count{0};
    for (;;) {
        bool initialized{false};
        udp_socket.send(create_ping_receiver_packet_bytes());
        ++ping_count;
        printf("Sent ping to %s:%d.\n", ip_address.c_str(), port);

        Sleep(100);
        
        while (auto packet = udp_socket.receive()) {
            int cursor{0};
            const int session_id{get_session_id_from_sender_packet_bytes(*packet)};
            const SenderPacketType packet_type{get_packet_type_from_sender_packet_bytes(*packet)};
            if (packet_type != SenderPacketType::Init) {
                printf("A different kind of a packet received before an init packet: %d\n", packet_type);
                continue;
            }

            sender_session_id = session_id;

            const auto init_sender_packet_data{parse_init_sender_packet_bytes(*packet)};
            depth_width = init_sender_packet_data.depth_width;
            depth_height = init_sender_packet_data.depth_height;

            initialized = true;
            break;
        }
        if (initialized)
            break;

        if (ping_count == 10) {
            printf("Tried pinging 10 times and failed to received an init packet...\n");
            return;
        }
    }

    bool stopped{false};
    moodycamel::ReaderWriterQueue<VideoSenderPacketData> video_packet_data_queue;
    moodycamel::ReaderWriterQueue<FecSenderPacketData> fec_packet_data_queue;
    moodycamel::ReaderWriterQueue<AudioSenderPacketData> audio_packet_data_queue;
    moodycamel::ReaderWriterQueue<std::pair<int, VideoSenderMessageData>> video_message_queue;
    int last_video_frame_id{-1};

    std::thread receive_sender_packets_thread([&] {
        receive_sender_packets(sender_session_id, stopped, udp_socket,
                                   video_packet_data_queue, fec_packet_data_queue,
                                   audio_packet_data_queue);
    });
    std::thread reassemble_video_messages_thread([&] {
        reassemble_video_messages(stopped, udp_socket, video_packet_data_queue,
                                  fec_packet_data_queue, video_message_queue, last_video_frame_id);
    });
    std::thread consume_audio_packets_thread([&] {
        consume_audio_packets(stopped, audio_packet_data_queue);
    });
    consume_video_message(stopped, depth_width, depth_height, udp_socket, video_message_queue, last_video_frame_id);

    receive_sender_packets_thread.join();
    reassemble_video_messages_thread.join();
    consume_audio_packets_thread.join();
}

void main()
{
    for (;;) {
        // Receive IP address from the user.
        printf("Enter an IP address to start receiving frames: ");
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        // Receive port from the user.
        printf("Enter a port number to start receiving frames: ");
        std::string port_line;
        std::getline(std::cin, port_line);
        // The default port is 7777.
        const int port{port_line.empty() ? 7777 : std::stoi(port_line)};
        receive_frames(ip_address, port);
    }
}
}

int main()
{
    kh::main();
    return 0;
}