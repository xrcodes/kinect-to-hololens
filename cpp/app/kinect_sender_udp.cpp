#include <chrono>
#include "kh_core.h"
#include "kh_sender.h"
#include "k4a/k4a.hpp"
#include "kh_vp8.h"
#include "kh_trvl.h"

namespace kh
{
int pow_of_two(int exp) {
    assert(exp >= 0);

    int res = 1;
    for (int i = 0; i < exp; ++i) {
        res *= 2;
    }
    return res;
}

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

// Sends Azure Kinect frames through a TCP port.
void _send_frames(int port, bool binned_depth)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const auto TIMEOUT = std::chrono::milliseconds(1000);

    std::cout << "binned_depth: " << binned_depth << std::endl;

    std::cout << "Start sending Azure Kinect frames (port: " << port << ")." << std::endl;

    auto device = k4a::device::open(K4A_DEVICE_DEFAULT);

    k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
    configuration.depth_mode = binned_depth ? K4A_DEPTH_MODE_NFOV_2X2BINNED : K4A_DEPTH_MODE_NFOV_UNBINNED;
    configuration.camera_fps = K4A_FRAMES_PER_SECOND_30;

    auto calibration = device.get_calibration(configuration.depth_mode, configuration.color_resolution);
    k4a::transformation transformation(calibration);

    Vp8Encoder color_encoder(calibration.depth_camera_calibration.resolution_width,
                             calibration.depth_camera_calibration.resolution_height,
                             TARGET_BITRATE);

    int depth_frame_width = calibration.depth_camera_calibration.resolution_width;
    int depth_frame_height = calibration.depth_camera_calibration.resolution_height;
    int depth_frame_size = depth_frame_width * depth_frame_height;
    TrvlEncoder depth_encoder(depth_frame_size, CHANGE_THRESHOLD, INVALID_THRESHOLD);

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

    std::array<char, 1> recv_buf;
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(recv_buf), remote_endpoint, 0, error);
    socket.non_blocking(true);

    if (error/* && error != boost::asio::error::message_size*/)
        throw std::system_error(error);

    std::cout << "Found a client!" << std::endl;

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    //Sender sender(std::move(socket));
    // The sender sends the KinectIntrinsics, so the renderer from the receiver side can prepare rendering Kinect frames.
    // TODO: Add a function send() for Azure Kinect.
    //sender.send(calibration);

    SenderUdp sender(std::move(socket), remote_endpoint);
    sender.send(calibration);

    device.start_cameras(&configuration);

    // frame_id is the ID of the frame the sender sends.
    int frame_id = 0;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id = 0;

    // Variables for profiling the sender.
    auto summary_start = std::chrono::system_clock::now();
    int frame_count = 0;
    std::chrono::microseconds latest_time_stamp;

    size_t frame_size = 0;
    for (;;) {
        auto frame_start = std::chrono::steady_clock::now();
        // Try receiving a frame ID from the receiver and update receiver_frame_id if possible.
        //auto receive_result = sender.receive();
        //if (receive_result) {
        //    int cursor = 0;

        //    // Currently, message_type is not getting used.
        //    auto message_type = (*receive_result)[0];
        //    cursor += 1;

        //    if (message_type == 0) {
        //        memcpy(&receiver_frame_id, receive_result->data() + cursor, 4);
        //    }
        //}

        //std::array<char, 5> buffer;
        //std::error_code error;
        //socket.receive_from(asio::buffer(buffer), remote_endpoint, 0, error);
        //uint8_t message_type = buffer[0];
        //memcpy(&receiver_frame_id, buffer.data() + 1, 4);

        auto receive_result = sender.receive();
        if (receive_result) {
            uint8_t message_type = (*receive_result)[0];
            memcpy(&receiver_frame_id, receive_result->data() + 1, 4);
        }

        auto capture_start = std::chrono::steady_clock::now();
        k4a::capture capture;
        if (!device.get_capture(&capture, TIMEOUT)) {
            std::cout << "get_capture() timed out" << std::endl;
            continue;
        }

        auto color_image = capture.get_color_image();
        if (!color_image) {
            std::cout << "no color_image" << std::endl;
            continue;
        }

        int frame_id_diff = frame_id - receiver_frame_id;

        auto time_stamp = color_image.get_device_timestamp();
        auto time_diff = time_stamp - latest_time_stamp;
        // Rounding assuming that the framerate is 30 Hz.
        int device_frame_diff = (int)(time_diff.count() / 33000.0f + 0.5f);

        // Skip frame if the receiver is struggling.
        // if frame_id_diff == 1 or 2 -> don't skip
        // if frame_id_diff == n -> skip 2 ^ (n - 3) frames.
        // Do not test for the first frame.
        if (frame_id != 0 && device_frame_diff < pow_of_two(frame_id_diff - 1) / 4) {
            continue;
        }

        auto depth_image = capture.get_depth_image();
        if (!depth_image) {
            std::cout << "no depth_image" << std::endl;
            continue;
        }

        float frame_time_stamp = time_stamp.count() / 1000.0f;

        auto transformation_start = std::chrono::steady_clock::now();
        auto transformed_color_image = transformation.color_image_to_depth_camera(depth_image, color_image);

        auto compression_start = std::chrono::steady_clock::now();
        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());
        auto vp8_frame = color_encoder.encode(yuv_image);

        auto depth_compression_start = std::chrono::steady_clock::now();
        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder.encode(reinterpret_cast<short*>(depth_image.get_buffer()));

        auto message_start = std::chrono::steady_clock::now();

        //auto message = get_frame_message(frame_id, frame_time_stamp, vp8_frame,
        //                                 reinterpret_cast<uint8_t*>(depth_encoder_frame.data()), depth_encoder_frame.size());
        //auto packets = split_frame_message(frame_id, message);

        auto send_start = std::chrono::steady_clock::now();

        //std::error_code send_error;
        //for (auto packet : packets) {
        //    sender.send(packet);
        //}
        sender.send(frame_id, frame_time_stamp, vp8_frame,
                    reinterpret_cast<uint8_t*>(depth_encoder_frame.data()), depth_encoder_frame.size());

        auto frame_end = std::chrono::steady_clock::now();

        // Per frame log.
        int byte_size = vp8_frame.size() + depth_encoder_frame.size();
        auto frame_time_diff = frame_end - frame_start;
        auto capture_time_diff = compression_start - capture_start;
        auto transformation_time = compression_start - transformation_start;
        auto compression_time = frame_end - compression_start;
        auto color_compression_time = depth_compression_start - compression_start;
        auto depth_compression_time = message_start - depth_compression_start;
        auto message_time = send_start - message_start;
        auto send_time = frame_end - send_start;

        std::cout << "frame_id: " << frame_id << ", "
                  << "frame_id_diff: " << frame_id_diff << ", "
                  //<< "byte_size: " << (byte_size / 1024) << " KB, "
                  //<< "frame_time_diff: " << (frame_time_diff.count() / 1000000) << " ms, "
                  //<< "capture_time_diff: " << (capture_time_diff.count() / 1000000) << " ms, "
                  //<< "transformation_time: " << (transformation_time.count() / 1000000) << " ms, "
                  //<< "compression_time: " << (compression_time.count() / 1000000) << " ms, "
                  << "color_compression_time: " << (color_compression_time.count() / 1000000) << " ms, "
                  << "depth_compression_time: " << (depth_compression_time.count() / 1000000) << " ms, "
                  << "send_time: " << (send_time.count() / 1000000) << " ms, "
                  //<< "time_diff: " << (time_diff.count() / 1000) << " ms, "
                  //<< "device_time_stamp: " << (time_stamp.count() / 1000) << " ms, "
                  << std::endl;

        latest_time_stamp = time_stamp;

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            auto summary_end = std::chrono::system_clock::now();
            std::chrono::duration<double> diff = summary_end - summary_start;
            std::stringstream ss;
            ss << "Summary for frame " << frame_id << ", "
                << "FPS: " << frame_count / diff.count() << ", "
                << "Bandwidth: " << frame_size / (diff.count() * 131072) << " Mbps. "; // 131072 = 1024 * 1024 / 8
             // White spaces are added at the end to make sure to clean up the previous line.
             // std::cout << ss.str() << "       \r";
            std::cout << ss.str() << std::endl;
            summary_start = summary_end;
            frame_count = 0;
            frame_size = 0;
        }

        ++frame_id;

        // Updating variables for profiling.
        ++frame_count;
        frame_size += vp8_frame.size() + depth_encoder_frame.size();
    }

    std::cout << "Stopped sending Kinect frames." << std::endl;
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void send_frames()
{
    for (;;) {
        std::cout << "Enter a port number to start sending frames: ";
        std::string line;
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        int port = line.empty() ? 7777 : std::stoi(line);

        std::cout << "Choose depth resolution (1: Full, 2: Half): ";
        std::getline(std::cin, line);

        // The default type is TRVL.
        bool binned_depth = false;
        if (line == "2")
            binned_depth = true;

        try {
            _send_frames(port, binned_depth);
        } catch (std::exception & e) {
            std::cout << e.what() << std::endl;
        }
    }
}
}

int main()
{
    kh::send_frames();
    return 0;
}