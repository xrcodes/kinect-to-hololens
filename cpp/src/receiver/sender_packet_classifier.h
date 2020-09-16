#pragma once

namespace kh
{
struct SenderPacketInfo
{
    bool received_any;
    std::vector<tt::VideoSenderPacket> video_packets;
    std::vector<tt::ParitySenderPacket> parity_packets;
    std::vector<tt::AudioSenderPacket> audio_packets;
};

class SenderPacketClassifier
{
public:
    static SenderPacketInfo classify(tt::UdpSocket& udp_socket)
    {
        SenderPacketInfo sender_packet_info;
        sender_packet_info.received_any = false;
        while (auto packet{udp_socket.receive(tt::KH_PACKET_SIZE)}) {
            sender_packet_info.received_any = true;
            switch (tt::get_packet_type_from_sender_packet_bytes(packet->bytes))
            {
            case tt::SenderPacketType::Heartbeat:
                break;
            case tt::SenderPacketType::Video:
                sender_packet_info.video_packets.push_back(tt::read_video_sender_packet(packet->bytes));
                break;
            case tt::SenderPacketType::Parity:
                sender_packet_info.parity_packets.push_back(tt::read_parity_sender_packet(packet->bytes));
                break;
            case tt::SenderPacketType::Audio:
                sender_packet_info.audio_packets.push_back(tt::read_audio_sender_packet(packet->bytes));
                break;
            }
        }

        return sender_packet_info;
    }
};
}