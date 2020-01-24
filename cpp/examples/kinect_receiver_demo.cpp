#include <iostream>
#include <asio.hpp>
#include "kh_receiver.h"
#include "kh_depth_compression_helper.h"
#include "helper/opencv_helper.h"

namespace kh
{
void _receive_azure_kinect_frames(std::string ip_address, int port)
{
    std::cout << "Try connecting to " << ip_address << ":" << port << std::endl;

    // Try connecting to a Sender with the IP address and the port.
    asio::io_context io_context;
    Receiver receiver(io_context);
    for (;;) {
        if (receiver.connect(ip_address, port))
            break;
    }

    std::cout << "Connected!" << std::endl;

    Vp8Decoder vp8_decoder;
    int depth_width;
    int depth_height;
    std::unique_ptr<DepthDecoder> depth_decoder;
    for (;;) {
        std::optional<int> last_frame_id;
        std::optional<kh::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;

        for (;;) {
            // Try receiving a message from the Receiver.
            auto receive_result = receiver.receive();
            // Keep trying again if there is none.
            if (!receive_result)
                break;

            int cursor = 0;
            auto message_type = (*receive_result)[0];
            cursor += 1;
            // There can be two types of messages: a KinectIntrinsics and a Kinect frame.
            if (message_type == 0) {
                std::cout << "Received a message for initialization." << std::endl;

                // for color width
                cursor += 4;
                // for color height
                cursor += 4;

                memcpy(&depth_width, receive_result->data() + cursor, 4);
                cursor += 4;

                memcpy(&depth_height, receive_result->data() + cursor, 4);
                cursor += 4;

                int depth_compression_type;
                memcpy(&depth_compression_type, receive_result->data() + cursor, 4);

                DepthCompressionType type = static_cast<DepthCompressionType>(depth_compression_type);
                if (type == DepthCompressionType::Rvl) {
                    depth_decoder = std::make_unique<RvlDepthDecoder>(depth_width * depth_height);
                } else if (type == DepthCompressionType::Trvl) {
                    depth_decoder = std::make_unique<TrvlDepthDecoder>(depth_width * depth_height);
                } else if (type == DepthCompressionType::Vp8) {
                    depth_decoder = std::make_unique<Vp8DepthDecoder>();
                }

            } else if (message_type == 1) {
                // Parse the ID of the frame and send a feedback meesage to the sender
                // to indicate the frame was succesfully received.
                // This is required to minimize the end-to-end latency from the Kinect of the Sender
                // and the display of the Receiver.
                int frame_id;
                memcpy(&frame_id, receive_result->data() + cursor, 4);
                cursor += 4;

                if (frame_id % 100 == 0)
                    std::cout << "Received frame " << frame_id << "." << std::endl;
                last_frame_id = frame_id;

                // Parsing the bytes of the message into the VP8 and RVL frames.
                int vp8_frame_size;
                memcpy(&vp8_frame_size, receive_result->data() + cursor, 4);
                cursor += 4;

                std::vector<uint8_t> vp8_frame(vp8_frame_size);
                memcpy(vp8_frame.data(), receive_result->data() + cursor, vp8_frame_size);
                cursor += vp8_frame_size;

                int depth_encoder_frame_size;
                memcpy(&depth_encoder_frame_size, receive_result->data() + cursor, 4);
                cursor += 4;

                std::vector<uint8_t> depth_encoder_frame(depth_encoder_frame_size);
                memcpy(depth_encoder_frame.data(), receive_result->data() + cursor, depth_encoder_frame_size);
                cursor += depth_encoder_frame_size;

                // Decoding a Vp8Frame into color pixels.
                ffmpeg_frame = vp8_decoder.decode(vp8_frame.data(), vp8_frame.size());

                // Decompressing a RVL frame into depth pixels.
                depth_image = depth_decoder->decode(depth_encoder_frame.data(), depth_encoder_frame.size());
            }
        }
        
        // If there was a frame meesage
        if (last_frame_id) {
            receiver.send(*last_frame_id);

            auto color_mat = createCvMatFromYuvImage(createYuvImageFromAvFrame(ffmpeg_frame->av_frame()));
            auto depth_mat = createCvMatFromKinectDepthImage(reinterpret_cast<uint16_t*>(depth_image.data()), depth_width, depth_height);

            // Rendering the depth pixels.
            cv::imshow("Color", color_mat);
            cv::imshow("Depth", depth_mat);
            if (cv::waitKey(1) >= 0)
                break;
        }
    }
}

void receive_frames()
{
    for (;;) {
        // Receive IP address from the user.
        std::cout << "Enter an IP address to start receiving frames: ";
        std::string ip_address;
        std::getline(std::cin, ip_address);
        // The default IP address is 127.0.0.1.
        if (ip_address.empty())
            ip_address = "127.0.0.1";

        // Receive port from the user.
        std::cout << "Enter a port number to start receiving frames: ";
        std::string port_line;
        std::getline(std::cin, port_line);
        // The default port is 7777.
        int port = port_line.empty() ? 7777 : std::stoi(port_line);
        try {
            _receive_azure_kinect_frames(ip_address, port);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }
}
}

int main()
{
    kh::receive_frames();
    return 0;
}