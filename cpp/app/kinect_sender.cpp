#include <chrono>
#include <iostream>
#include <random>
#include "readerwriterqueue/readerwriterqueue.h"
#include "helper/kinect_helper.h"
#include "kh_sender.h"
#include "kh_trvl.h"
#include "kh_vp8.h"

namespace kh
{
class FramePacketSet
{
public:
    FramePacketSet()
        : frame_id_(0), packets_()
    {
    }
    FramePacketSet(int frame_id, std::vector<std::vector<uint8_t>>&& packets)
        : frame_id_(frame_id), packets_(std::move(packets))
    {
    }
    int frame_id() { return frame_id_; }
    std::vector<std::vector<uint8_t>>& packets() { return packets_; }

private:
    int frame_id_;
    std::vector<std::vector<uint8_t>> packets_;
};

int pow_of_two(int exp) {
    assert(exp >= 0);

    int res = 1;
    for (int i = 0; i < exp; ++i) {
        res *= 2;
    }
    return res;
}

void run_sender_thread(bool& stop_sender_thread,
                       Sender& sender,
                       moodycamel::ReaderWriterQueue<FramePacketSet>& frame_packet_queue,
                       int& receiver_frame_id)
{
    std::unordered_map<int, std::chrono::time_point<std::chrono::steady_clock>> frame_send_times;
    std::unordered_map<int, FramePacketSet> frame_packet_sets;
    int last_receiver_frame_id = 0;
    auto send_summary_start = std::chrono::steady_clock::now();
    int send_summary_receiver_frame_count = 0;
    int send_summary_receiver_packet_count = 0;
    int send_summary_packet_count = 0;
    while (!stop_sender_thread) {
        auto receive_result = sender.receive();
        if (receive_result) {
            int cursor = 0;
            uint8_t message_type = (*receive_result)[cursor];
            cursor += 1;

            if (message_type == 1) {
                memcpy(&receiver_frame_id, receive_result->data() + cursor, 4);
                cursor += 4;

                float packet_collection_time_ms;
                memcpy(&packet_collection_time_ms, receive_result->data() + cursor, 4);
                cursor += 4;

                float decoder_time_ms;
                memcpy(&decoder_time_ms, receive_result->data() + cursor, 4);
                cursor += 4;

                float frame_time_ms;
                memcpy(&frame_time_ms, receive_result->data() + cursor, 4);
                cursor += 4;

                int receiver_packet_count;
                memcpy(&receiver_packet_count, receive_result->data() + cursor, 4);

                std::chrono::duration<double> round_trip_time = std::chrono::steady_clock::now() - frame_send_times[receiver_frame_id];

                printf("Frame id: %d, packet: %f ms, decoder: %f ms, frame: %f ms, round_trip: %f ms\n",
                       receiver_frame_id, packet_collection_time_ms, decoder_time_ms, frame_time_ms,
                       round_trip_time.count() * 1000.0f);

                std::vector<int> obsolete_frame_ids;
                for (auto& frame_send_time_pair : frame_send_times) {
                    if (frame_send_time_pair.first <= receiver_frame_id)
                        obsolete_frame_ids.push_back(frame_send_time_pair.first);
                }

                for (int obsolete_frame_id : obsolete_frame_ids)
                    frame_send_times.erase(obsolete_frame_id);

                ++send_summary_receiver_frame_count;
                send_summary_receiver_packet_count += receiver_packet_count;
            } else if (message_type == 2) {
                int requested_frame_id;
                memcpy(&requested_frame_id, receive_result->data() + cursor, 4);
                cursor += 4;
                
                int missing_packet_count;
                memcpy(&missing_packet_count, receive_result->data() + cursor, 4);
                cursor += 4;
                
                for (int i = 0; i < missing_packet_count; ++i) {
                    int missing_packet_id;
                    memcpy(&missing_packet_id, receive_result->data() + cursor, 4);
                    cursor += 4;

                    //missing_packet_ids.push_back(missing_packet_id);
                    auto it = frame_packet_sets.find(requested_frame_id);
                    if (it == frame_packet_sets.end())
                        continue;

                    sender.sendPacket(frame_packet_sets[requested_frame_id].packets()[missing_packet_id]);
                    ++send_summary_packet_count;
                }
            }
        }

        FramePacketSet frame_packet_set;
        while (frame_packet_queue.try_dequeue(frame_packet_set)) {
            frame_send_times[frame_packet_set.frame_id()] = std::chrono::steady_clock::now();
            for (auto packet : frame_packet_set.packets()) {
                try {
                    sender.sendPacket(packet);
                    ++send_summary_packet_count;
                } catch (std::system_error e) {
                    if (e.code() == asio::error::would_block) {
                        printf("Failed to send frame as the buffer was full...\n");
                    } else {
                        throw e;
                    }
                }
            }
            frame_packet_sets[frame_packet_set.frame_id()] = std::move(frame_packet_set);
        }

        // Remove elements of frame_packet_sets reserved for filling up missing packets
        // if they are already used from the receiver side.
        std::vector<int> obsolete_frame_ids;
        for (auto& frame_packet_set_pair : frame_packet_sets) {
            if (frame_packet_set_pair.first <= receiver_frame_id)
                obsolete_frame_ids.push_back(frame_packet_set_pair.first);
        }

        for (int obsolete_frame_id : obsolete_frame_ids)
            frame_packet_sets.erase(obsolete_frame_id);

        if ((receiver_frame_id / 100) > (last_receiver_frame_id / 100)) {
            std::chrono::duration<double> send_summary_time_interval = std::chrono::steady_clock::now() - send_summary_start;
            float packet_loss = 1.0f - send_summary_receiver_packet_count / (float)send_summary_packet_count;
            printf("Send Summary: Receiver FPS: %lf, Packet Loss: %f%%\n",
                   send_summary_receiver_frame_count / send_summary_time_interval.count(),
                   packet_loss * 100.0f);

            send_summary_start = std::chrono::steady_clock::now();
            send_summary_receiver_frame_count = 0;
            send_summary_packet_count = 0;
            send_summary_receiver_packet_count = 0;
        }
        last_receiver_frame_id = receiver_frame_id;
    }
}

void send_frames(int session_id, KinectDevice& device, int port)
{
    const int TARGET_BITRATE = 2000;
    const short CHANGE_THRESHOLD = 10;
    const int INVALID_THRESHOLD = 2;
    const int SENDER_SEND_BUFFER_SIZE = 1024 * 1024;
    //const int SENDER_SEND_BUFFER_SIZE = 128 * 1024;

    printf("Start Sending Frames (session_id: %d, port: %d)\n", session_id, port);

    auto calibration = device.getCalibration();
    k4a::transformation transformation(calibration);

    int depth_width = calibration.depth_camera_calibration.resolution_width;
    int depth_height = calibration.depth_camera_calibration.resolution_height;
    
    // Color encoder also uses the depth width/height since color pixels get transformed to the depth camera.
    Vp8Encoder color_encoder(depth_width, depth_height, TARGET_BITRATE);
    TrvlEncoder depth_encoder(depth_width * depth_height, CHANGE_THRESHOLD, INVALID_THRESHOLD);

    asio::io_context io_context;
    asio::ip::udp::socket socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

    std::vector<uint8_t> ping_buffer(1);
    asio::ip::udp::endpoint remote_endpoint;
    std::error_code error;
    socket.receive_from(asio::buffer(ping_buffer), remote_endpoint, 0, error);

    if (error) {
        printf("Error receiving ping: %s\n", error.message().c_str());
        throw std::system_error(error);
    }

    printf("Found a Receiver at %s:%d\n", remote_endpoint.address().to_string().c_str(), remote_endpoint.port());

    // Sender is a class that will use the socket to send frames to the receiver that has the socket connected to this socket.
    Sender sender(std::move(socket), remote_endpoint, SENDER_SEND_BUFFER_SIZE);
    sender.send(session_id, calibration);

    bool stop_sender_thread = false;
    moodycamel::ReaderWriterQueue<FramePacketSet> frame_packet_queue;
    // receiver_frame_id is the ID that the receiver sent back saying it received the frame of that ID.
    int receiver_frame_id = 0;
    std::thread sender_thread(run_sender_thread, std::ref(stop_sender_thread), std::ref(sender),
                              std::ref(frame_packet_queue), std::ref(receiver_frame_id));
    
    // frame_id is the ID of the frame the sender sends.
    int frame_id = 0;

    // Variables for profiling the sender.
    int main_summary_keyframe_count = 0;
    std::chrono::microseconds last_time_stamp;

    auto main_summary_start = std::chrono::steady_clock::now();
    size_t main_summary_frame_size_sum = 0;
    for (;;) {
        auto capture = device.getCapture();
        if (!capture)
            continue;

        auto color_image = capture->get_color_image();
        if (!color_image) {
            printf("get_color_image() failed...\n");
            continue;
        }

        auto time_stamp = color_image.get_device_timestamp();
        auto time_diff = time_stamp - last_time_stamp;
        float frame_time_stamp = time_stamp.count() / 1000.0f;
        int frame_id_diff = frame_id - receiver_frame_id;
        int device_frame_diff = (int)(time_diff.count() / 33000.0f + 0.5f);
        //if (frame_id != 0 && device_frame_diff < pow_of_two(frame_id_diff - 1) / 4) {
        //    continue;
        //}

        auto depth_image = capture->get_depth_image();
        if (!depth_image) {
            printf("get_depth_image() failed...\n");
            continue;
        }

        bool keyframe = frame_id_diff > 4;

        auto transformed_color_image = transformation.color_image_to_depth_camera(depth_image, color_image);

        // Format the color pixels from the Kinect for the Vp8Encoder then encode the pixels with Vp8Encoder.
        auto yuv_image = createYuvImageFromAzureKinectBgraBuffer(transformed_color_image.get_buffer(),
                                                                 transformed_color_image.get_width_pixels(),
                                                                 transformed_color_image.get_height_pixels(),
                                                                 transformed_color_image.get_stride_bytes());
        auto vp8_frame = color_encoder.encode(yuv_image, keyframe);

        // Compress the depth pixels.
        auto depth_encoder_frame = depth_encoder.encode(reinterpret_cast<short*>(depth_image.get_buffer()), keyframe);

        auto message = Sender::createFrameMessage(frame_time_stamp, keyframe, vp8_frame,
                                                    reinterpret_cast<uint8_t*>(depth_encoder_frame.data()),
                                                    static_cast<uint32_t>(depth_encoder_frame.size()));
        auto packets = Sender::splitFrameMessage(session_id, frame_id, message);
        frame_packet_queue.enqueue(FramePacketSet(frame_id, std::move(packets)));

        last_time_stamp = time_stamp;

        // Updating variables for profiling.
        if (keyframe)
            ++main_summary_keyframe_count;
        main_summary_frame_size_sum += (vp8_frame.size() + depth_encoder_frame.size());

        // Print profile measures every 100 frames.
        if (frame_id % 100 == 0) {
            std::chrono::duration<double> main_summary_time_interval = std::chrono::steady_clock::now() - main_summary_start;
            printf("Main Summary id: %d, FPS: %lf, Keyframe Ratio: %d%%, Bandwidth: %lf Mbps\n",
                   frame_id,
                   100 / main_summary_time_interval.count(),
                   main_summary_keyframe_count,
                   main_summary_frame_size_sum / (main_summary_time_interval.count() * 131072));

            main_summary_start = std::chrono::steady_clock::now();
            main_summary_keyframe_count = 0;
            main_summary_frame_size_sum = 0;
        }
        ++frame_id;
    }
    stop_sender_thread = true;
    sender_thread.join();
}

// Repeats collecting the port number from the user and calling _send_frames() with it.
void main()
{
    srand(time(nullptr));
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

    for (;;) {
        std::string line;
        printf("Enter a port number to start sending frames: ");
        std::getline(std::cin, line);
        // The default port (the port when nothing is entered) is 7777.
        int port = line.empty() ? 7777 : std::stoi(line);

        k4a_device_configuration_t configuration = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
        configuration.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
        configuration.color_resolution = K4A_COLOR_RESOLUTION_720P;
        configuration.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
        auto timeout = std::chrono::milliseconds(1000);

        auto device = KinectDevice::create(configuration, timeout);
        if (!device) {
            printf("Failed to create a KinectDevice...\n");
            continue;
        }
        device->start();

        int session_id = rng() % (INT_MAX + 1);

        try {
            send_frames(session_id, *device, port);
        } catch (std::exception & e) {
            printf("Error from _send_frames: %s\n", e.what());
        }
    }
}
}

int main()
{
    kh::main();
    return 0;
}