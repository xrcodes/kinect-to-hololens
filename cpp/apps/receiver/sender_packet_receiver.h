#pragma once

namespace kh
{
struct SenderPacketSet
{
    bool received_any;
    std::vector<VideoSenderPacket> video_packets;
    std::vector<ParitySenderPacket> parity_packets;
    std::vector<AudioSenderPacket> audio_packets;
};

class SenderPacketReceiver
{
public:
    static SenderPacketSet receive(UdpSocket& udp_socket)
    {
        SenderPacketSet sender_packet_set;
        sender_packet_set.received_any = false;
        while (auto packet{udp_socket.receive()}) {
            sender_packet_set.received_any = true;
            switch (get_packet_type_from_sender_packet_bytes(packet->bytes))
            {
            case SenderPacketType::Heartbeat:
                break;
            case SenderPacketType::Video:
                sender_packet_set.video_packets.push_back(read_video_sender_packet(packet->bytes));
                break;
            case SenderPacketType::Parity:
                sender_packet_set.parity_packets.push_back(read_parity_sender_packet(packet->bytes));
                break;
            case SenderPacketType::Audio:
                sender_packet_set.audio_packets.push_back(read_audio_sender_packet(packet->bytes));
                break;
            }
        }

        return sender_packet_set;
    }
};
}