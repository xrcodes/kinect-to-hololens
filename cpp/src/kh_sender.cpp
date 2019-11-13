#include "kh_sender.h"

#include "kh_message.h"

namespace kh
{
// Receives a moved socket.
Sender::Sender(asio::ip::tcp::socket&& socket)
    : socket_(std::move(socket))
{
    // It is unncessary for the current version of our applications to have a non-blocking socket.
    // However, it might befuture as multi-threading might happen.
    socket_.non_blocking(true);
}

// Sends a Kinect calibration information to a Receiver.
void Sender::send(k4a_calibration_t calibration)
{
    auto color_intrinsics = calibration.color_camera_calibration.intrinsics.parameters.param;
    auto depth_intrinsics = calibration.depth_camera_calibration.intrinsics.parameters.param;
    auto depth_to_color_extrinsics = calibration.extrinsics[K4A_CALIBRATION_TYPE_DEPTH][K4A_CALIBRATION_TYPE_COLOR];

    uint32_t message_size = static_cast<uint32_t>(1 + sizeof(color_intrinsics) + sizeof(depth_intrinsics) +
                                                  sizeof(depth_to_color_extrinsics));
    uint32_t buffer_size = static_cast<uint32_t>(4 + message_size);

    std::vector<uint8_t> buffer(buffer_size);
    size_t cursor = 0;

    memcpy(buffer.data() + cursor, &message_size, 4);
    cursor += 4;

    // Message type
    buffer[4] = static_cast<uint8_t>(0);
    cursor += 1;

    memcpy(buffer.data() + cursor, &color_intrinsics, sizeof(color_intrinsics));
    cursor += sizeof(color_intrinsics);

    memcpy(buffer.data() + cursor, &depth_intrinsics, sizeof(depth_intrinsics));
    cursor += sizeof(depth_intrinsics);

    memcpy(buffer.data() + cursor, &depth_to_color_extrinsics, sizeof(depth_to_color_extrinsics));

    sendMessageBuffer(socket_, buffer);
}

// Sends a Kinect frame to a Receiver.
void Sender::send(int frame_id, std::vector<uint8_t>& vp8_frame, std::vector<uint8_t> rvl_frame)
{
    uint32_t message_size = static_cast<uint32_t>(1 + 4 + 4 + vp8_frame.size() + 4 + rvl_frame.size());
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

    auto encoder_frame_size = static_cast<uint32_t>(vp8_frame.size());
    memcpy(buffer.data() + cursor, &encoder_frame_size, 4);
    cursor += 4;

    memcpy(buffer.data() + cursor, vp8_frame.data(), vp8_frame.size());
    cursor += vp8_frame.size();

    auto rvl_frame_size = static_cast<uint32_t>(rvl_frame.size());
    memcpy(buffer.data() + cursor, &rvl_frame_size, 4);
    cursor += 4;

    memcpy(buffer.data() + cursor, rvl_frame.data(), rvl_frame.size());

    sendMessageBuffer(socket_, buffer);
}

// Receives a message from a Receiver that includes an ID of a Kinect frame that was sent.
std::optional<std::vector<uint8_t>> Sender::receive()
{
    return message_buffer_.receive(socket_);
}
}