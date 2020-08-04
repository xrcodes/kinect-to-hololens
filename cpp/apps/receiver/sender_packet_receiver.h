#pragma once

namespace kh
{
struct SenderPacketSet
{
    bool received_any;
    std::vector<VideoInitSenderPacketData> video_init_packet_data_vector;
    std::vector<VideoSenderPacketData> video_packet_data_vector;
    std::vector<ParitySenderPacketData> fec_packet_data_vector;
    std::vector<AudioSenderPacketData> audio_packet_data_vector;
};

class SenderPacketReceiver
{
public:
    static SenderPacketSet receive(UdpSocket& udp_socket)
    {
        SenderPacketSet sender_packet_set;
        sender_packet_set.received_any = false;
        while (auto packet{udp_socket.receive()}) {
            //const int session_id{get_session_id_from_sender_packet_bytes(packet->bytes)};
            sender_packet_set.received_any = true;
            switch (get_packet_type_from_sender_packet_bytes(packet->bytes))
            {
            case SenderPacketType::VideoInit:
                sender_packet_set.video_init_packet_data_vector.push_back(parse_video_init_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Video:
                sender_packet_set.video_packet_data_vector.push_back(parse_video_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Parity:
                sender_packet_set.fec_packet_data_vector.push_back(parse_parity_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Audio:
                sender_packet_set.audio_packet_data_vector.push_back(parse_audio_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Floor:
                // Ignore
                break;
            }
        }

        return sender_packet_set;
    }
};
}