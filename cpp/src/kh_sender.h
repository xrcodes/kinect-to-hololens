#pragma once

#include <iostream>
#include <asio.hpp>
#include "k4a/k4a.h"
#include "kh_message.h"

namespace kh
{
// Sends KinectIntrinsics and Kinect frames to a Receiver using the socket_.
// Receives socket_ through its constructor.
// Can receive messages from the Receiver that Kinect frames were successfully sent.
class Sender
{
public:
    Sender(asio::ip::tcp::socket&& socket);
    void send(k4a_calibration_t calibration);
    void send(int frame_id, float frame_time_stamp, std::vector<uint8_t>& vp8_frame, uint8_t* rvl_frame, uint32_t rvl_frame_size);
    std::optional<std::vector<uint8_t>> receive();

private:
    asio::ip::tcp::socket socket_;
    MessageBuffer message_buffer_;
};
}