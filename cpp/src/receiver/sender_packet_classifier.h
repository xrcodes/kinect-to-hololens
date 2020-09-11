#pragma once

namespace kh
{
struct SenderPacketInfo
{
    bool received_any;
    std::vector<VideoSenderPacket> video_packets;
    std::vector<ParitySenderPacket> parity_packets;
    std::vector<AudioSenderPacket> audio_packets;
};

class SenderPacketClassifier
{
public:
    static SenderPacketInfo categorizePackets(UdpSocket& udp_socket)
    {
        SenderPacketInfo sender_packet_info;
        sender_packet_info.received_any = false;
        while (auto packet{udp_socket.receive()}) {
            sender_packet_info.received_any = true;
            switch (get_packet_type_from_sender_packet_bytes(packet->bytes))
            {
            case SenderPacketType::Heartbeat:
                break;
            case SenderPacketType::Video:
                sender_packet_info.video_packets.push_back(read_video_sender_packet(packet->bytes));
                break;
            case SenderPacketType::Parity:
                sender_packet_info.parity_packets.push_back(read_parity_sender_packet(packet->bytes));
                break;
            case SenderPacketType::Audio:
                sender_packet_info.audio_packets.push_back(read_audio_sender_packet(packet->bytes));
                break;
            }
        }

        return sender_packet_info;
    }
};
}