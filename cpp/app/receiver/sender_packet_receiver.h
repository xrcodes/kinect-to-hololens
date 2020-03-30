#pragma once

namespace kh
{
class SenderPacketReceiver
{
public:
    SenderPacketReceiver()
        : video_packet_data_queue_{}, fec_packet_data_queue_{}, audio_packet_data_queue_{}
    {
    }

    void receive(int sender_session_id, UdpSocket& udp_socket)
    {
        while (auto packet{udp_socket.receive()}) {
            const int session_id{get_session_id_from_sender_packet_bytes(packet->bytes)};

            if (session_id != sender_session_id)
                continue;

            switch (get_packet_type_from_sender_packet_bytes(packet->bytes))
            {
            case SenderPacketType::Video:
                video_packet_data_queue_.enqueue(parse_video_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Fec:
                fec_packet_data_queue_.enqueue(parse_fec_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Audio:
                audio_packet_data_queue_.enqueue(parse_audio_sender_packet_bytes(packet->bytes));
                break;
            case SenderPacketType::Floor:
                // Ignore
                break;
            }
        }
    }

    moodycamel::ReaderWriterQueue<VideoSenderPacketData>& video_packet_data_queue() { return video_packet_data_queue_; }
    moodycamel::ReaderWriterQueue<FecSenderPacketData>& fec_packet_data_queue() { return fec_packet_data_queue_; }
    moodycamel::ReaderWriterQueue<AudioSenderPacketData>& audio_packet_data_queue() { return audio_packet_data_queue_; }

private:
    moodycamel::ReaderWriterQueue<VideoSenderPacketData> video_packet_data_queue_;
    moodycamel::ReaderWriterQueue<FecSenderPacketData> fec_packet_data_queue_;
    moodycamel::ReaderWriterQueue<AudioSenderPacketData> audio_packet_data_queue_;
};
}