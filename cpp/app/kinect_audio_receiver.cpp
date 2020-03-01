#include <algorithm>
#include <iostream>
#include <map>
#include <asio.hpp>
#include "kh_opus.h"
#include "native/kh_time.h"
#include "native/kh_udp_socket.h"
#include "native/kh_packet.h"
#include "helper/soundio_helper.h"

namespace kh
{
int main(std::string ip_address, int port)
{
    constexpr int RECEIVER_RECEIVE_BUFFER_SIZE = 1024 * 1024;

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

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());
    socket.set_option(asio::socket_base::receive_buffer_size{RECEIVER_RECEIVE_BUFFER_SIZE});

    UdpSocket udp_socket(std::move(socket), asio::ip::udp::endpoint{asio::ip::address::from_string(ip_address), gsl::narrow_cast<unsigned short>(port)});
    std::error_code asio_error;
    udp_socket.send(create_ping_receiver_packet_bytes(), asio_error);

    AudioDecoder audio_decoder{KH_SAMPLE_RATE, KH_CHANNEL_COUNT};

    default_speaker_stream.start();

    std::array<float, KH_SAMPLES_PER_FRAME * KH_CHANNEL_COUNT> pcm;

    std::vector<AudioSenderPacketData> audio_packet_data_set;
    int received_byte_count{0};
    int last_frame_id{-1};
    auto summary_time{TimePoint::now()};
    for (;;) {
        soundio_flush_events(audio.get());

        while (auto packet = udp_socket.receive(asio_error)) {
            received_byte_count += packet->size();
            
            audio_packet_data_set.push_back(parse_audio_sender_packet_bytes(*packet));
        }

        std::sort(audio_packet_data_set.begin(),
                  audio_packet_data_set.end(),
                  [](AudioSenderPacketData& a, AudioSenderPacketData& b) { return a.frame_id < b.frame_id; });

        char* write_ptr = soundio_ring_buffer_write_ptr(soundio_callback::ring_buffer);
        int free_bytes = soundio_ring_buffer_free_count(soundio_callback::ring_buffer);

        constexpr int FRAME_BYTE_SIZE{sizeof(float) * pcm.size()};

        int write_cursor = 0;
        auto packet_it = audio_packet_data_set.begin();
        while((free_bytes - write_cursor) >= FRAME_BYTE_SIZE) {
            if (packet_it == audio_packet_data_set.end())
                break;

            int frame_size;
            if (packet_it->frame_id <= last_frame_id) {
                // If a packet is about the past, throw it away and try again.
                packet_it = audio_packet_data_set.erase(packet_it);
                continue;
            } else if (packet_it->frame_id == last_frame_id + 1) {
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

            ++last_frame_id;
            write_cursor += FRAME_BYTE_SIZE;
        }

        soundio_ring_buffer_advance_write_ptr(soundio_callback::ring_buffer, write_cursor);

        auto summary_diff{TimePoint::now() - summary_time};
        if (summary_diff.sec() > 5)
        {
            printf("Bandwidth: %f Mbps\n", (received_byte_count / (1024.0f * 1024.0f / 8.0f)) / summary_diff.sec());
            received_byte_count = 0;
            summary_time = TimePoint::now();
        }
    }
    return 0;
}
}

int main()
{
    return kh::main("127.0.0.1", 7777);
}