#include <iostream>
#include <asio.hpp>

class FramePacketCollection
{
public:
    FramePacketCollection()
    {
    }

    FramePacketCollection(int frame_id, int packet_count)
        : frame_id_(frame_id), packet_count_(packet_count), packets_(packet_count_)
    {
    }

    void addPacket(int packet_index, std::vector<uint8_t> packet)
    {
        packets_[packet_index] = packet;
    }

    bool isFull()
    {
        for (auto packet : packets_) {
            if (packet.empty())
                return false;
        }

        return true;
    }

private:
    int frame_id_;
    int packet_count_;
    std::vector<std::vector<std::uint8_t>> packets_;
};

int main(int argc, char* argv[])
{
    asio::io_context io_context;

    asio::ip::udp::resolver resolver(io_context);
    asio::ip::udp::endpoint receiver_endpoint =
        *resolver.resolve(asio::ip::udp::v4(), "127.0.0.1", "7777").begin();

    asio::ip::udp::socket socket(io_context);
    socket.open(asio::ip::udp::v4());

    std::array<char, 1> send_buf = { { 0 } };
    socket.send_to(asio::buffer(send_buf), receiver_endpoint);

    std::cout << "sent endpoint" << std::endl;

    std::unordered_map<int, FramePacketCollection> frame_packet_collections;
    for (;;) {
        std::vector<std::vector<uint8_t>> packets;
        std::vector<uint8_t> packet(1500);
        asio::ip::udp::endpoint sender_endpoint;
        size_t packet_size = socket.receive_from(asio::buffer(packet), sender_endpoint);
        packet.resize(packet_size);
        packets.push_back(packet);

        for (auto packet : packets) {
            uint8_t packet_type = packet[0];
            int frame_id;
            int packet_index;
            int packet_count;
            memcpy(&frame_id, packet.data() + 1, 4);
            memcpy(&packet_index, packet.data() + 5, 4);
            memcpy(&packet_count, packet.data() + 9, 4);

            auto it = frame_packet_collections.find(frame_id);
            if (it == frame_packet_collections.end()) {
                frame_packet_collections[frame_id] = FramePacketCollection(frame_id, packet_count);
            }

            frame_packet_collections[frame_id].addPacket(packet_index, packet);

            std::vector<int> full_frame_ids;
            for (auto collection_pair : frame_packet_collections) {
                if (collection_pair.second.isFull()) {
                    int frame_id = collection_pair.first;
                    full_frame_ids.push_back(frame_id);
                }
            }

            for (int full_frame_id : full_frame_ids) {
                frame_packet_collections.erase(full_frame_id);
                std::cout << "frame_id: " << frame_id << std::endl;

                std::array<char, 5> frame_id_buffer;
                frame_id_buffer[0] = 1;
                memcpy(frame_id_buffer.data() + 1, &full_frame_id, 4);
                socket.send_to(asio::buffer(frame_id_buffer), receiver_endpoint);
            }

            //std::cout << "frame_id: " << frame_id << ", packet_index: " << packet_index << ", packet_count: " << packet_count << std::endl;
        }
    }

    return 0;
}