#pragma once

#include <iostream>
#include <asio.hpp>
#include <optional>
#include "k4a/k4a.hpp"

namespace kh
{
class SenderUdp
{
public:
    SenderUdp(asio::ip::udp::socket&& socket, asio::ip::udp::endpoint remote_endpoint)
        : socket_(std::move(socket)), remote_endpoint_(remote_endpoint)
    {
        socket_.non_blocking(true);
    }

    // Sends a Kinect calibration information to a Receiver.
    void send(k4a_calibration_t calibration)
    {
        auto depth_intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
        int depth_width = calibration.depth_camera_calibration.resolution_width;
        int depth_height = calibration.depth_camera_calibration.resolution_height;
        float depth_metric_radius = calibration.depth_camera_calibration.metric_radius;

        auto color_intrinsics = calibration.color_camera_calibration.intrinsics.parameters.param;
        int color_width = calibration.color_camera_calibration.resolution_width;
        int color_height = calibration.color_camera_calibration.resolution_height;
        float color_metric_radius = calibration.color_camera_calibration.metric_radius;

        auto depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

        uint32_t message_size = static_cast<uint32_t>(1 +
                                                      sizeof(color_width) +
                                                      sizeof(color_height) +
                                                      sizeof(depth_width) +
                                                      sizeof(depth_height) +
                                                      sizeof(color_intrinsics) +
                                                      sizeof(color_metric_radius) +
                                                      sizeof(depth_intrinsics) +
                                                      sizeof(depth_metric_radius) +
                                                      sizeof(depth_to_color_extrinsics));

        std::vector<uint8_t> message(message_size);
        size_t cursor = 0;

        // Message type
        message[0] = static_cast<uint8_t>(0);
        cursor += 1;

        memcpy(message.data() + cursor, &color_width, sizeof(color_width));
        cursor += sizeof(color_width);

        memcpy(message.data() + cursor, &color_height, sizeof(color_height));
        cursor += sizeof(color_height);

        memcpy(message.data() + cursor, &depth_width, sizeof(depth_width));
        cursor += sizeof(depth_width);

        memcpy(message.data() + cursor, &depth_height, sizeof(depth_height));
        cursor += sizeof(depth_height);

        memcpy(message.data() + cursor, &color_intrinsics, sizeof(color_intrinsics));
        cursor += sizeof(color_intrinsics);

        memcpy(message.data() + cursor, &color_metric_radius, sizeof(color_metric_radius));
        cursor += sizeof(color_metric_radius);

        memcpy(message.data() + cursor, &depth_intrinsics, sizeof(depth_intrinsics));
        cursor += sizeof(depth_intrinsics);

        memcpy(message.data() + cursor, &depth_metric_radius, sizeof(depth_metric_radius));
        cursor += sizeof(depth_metric_radius);

        memcpy(message.data() + cursor, &depth_to_color_extrinsics, sizeof(depth_to_color_extrinsics));

        sendPacket(message);
    }

    void send(int frame_id, float frame_time_stamp, std::vector<uint8_t>& vp8_frame,
              uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size)
    {
        auto message = createFrameMessage(frame_id, frame_time_stamp, vp8_frame, depth_encoder_frame, depth_encoder_frame_size);
        auto packets = splitFrameMessage(frame_id, message);
        for (auto packet : packets) {
            sendPacket(packet);
        }
    }

    std::vector<uint8_t> createFrameMessage(int frame_id, float frame_time_stamp, std::vector<uint8_t>& vp8_frame,
                                            uint8_t* depth_encoder_frame, uint32_t depth_encoder_frame_size)
    {
        uint32_t message_size = static_cast<uint32_t>(1 + 4 + 4 + 4 + vp8_frame.size() + 4 + depth_encoder_frame_size);
        uint32_t buffer_size = static_cast<uint32_t>(4 + message_size);

        std::vector<uint8_t> buffer(buffer_size);
        size_t cursor = 0;

        memcpy(buffer.data() + cursor, &message_size, 4);
        cursor += 4;

        // Message type
        buffer[4] = static_cast<uint8_t>(1);
        cursor += 1;

        memcpy(buffer.data() + cursor, &frame_id, 4);
        cursor += 4;

        memcpy(buffer.data() + cursor, &frame_time_stamp, 4);
        cursor += 4;

        auto encoder_frame_size = static_cast<uint32_t>(vp8_frame.size());
        memcpy(buffer.data() + cursor, &encoder_frame_size, 4);
        cursor += 4;

        memcpy(buffer.data() + cursor, vp8_frame.data(), vp8_frame.size());
        cursor += vp8_frame.size();

        memcpy(buffer.data() + cursor, &depth_encoder_frame_size, 4);
        cursor += 4;

        memcpy(buffer.data() + cursor, depth_encoder_frame, depth_encoder_frame_size);

        return buffer;
    }

    std::vector<std::vector<uint8_t>> splitFrameMessage(int frame_id, std::vector<uint8_t> frame_message)
    {
        const int MAX_UDP_PACKET_SIZE = 1500;
        const int FRAME_PACKET_HEADER_SIZE = 13;
        const int MAX_FRAME_PACKET_CONTENT_SIZE = MAX_UDP_PACKET_SIZE - FRAME_PACKET_HEADER_SIZE;

        int packet_count = (frame_message.size() - 1) / MAX_FRAME_PACKET_CONTENT_SIZE + 1;
        std::vector<std::vector<uint8_t>> packets;
        for (int packet_index = 0; packet_index < packet_count; ++packet_index) {
            int message_cursor = MAX_FRAME_PACKET_CONTENT_SIZE * packet_index;

            int packet_content_size = MAX_FRAME_PACKET_CONTENT_SIZE;
            if ((packet_index + 1) == packet_count) {
                packet_content_size = frame_message.size() - message_cursor;
            }

            std::vector<uint8_t> packet(packet_content_size + FRAME_PACKET_HEADER_SIZE);
            uint8_t message_type = 1;
            memcpy(packet.data() + 0, &message_type, 1);
            memcpy(packet.data() + 1, &frame_id, 4);
            memcpy(packet.data() + 5, &packet_index, 4);
            memcpy(packet.data() + 9, &packet_count, 4);
            memcpy(packet.data() + 13, frame_message.data() + message_cursor, packet_content_size);
            packets.push_back(packet);
        }

        return packets;
    }

    void sendPacket(const std::vector<uint8_t>& packet)
    {
        std::error_code error;
        socket_.send_to(asio::buffer(packet), remote_endpoint_, 0, error);
        if (error)
            std::cout << "Error from SenderUdp::send(): " << error.message() << std::endl;
    }

    std::optional<std::vector<uint8_t>> receive()
    {
        std::vector<uint8_t> packet(1500);
        asio::ip::udp::endpoint sender_endpoint;
        std::error_code error;
        size_t packet_size = socket_.receive_from(asio::buffer(packet), sender_endpoint, 0, error);

        if (error == asio::error::would_block) {
            return std::nullopt;
        }

        if (error) {
            std::cout << "Error from SenderUdp::receive(): " << error.message() << std::endl;
            return std::nullopt;
        }

        packet.resize(packet_size);
        return packet;
    }

private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint remote_endpoint_;
};
}