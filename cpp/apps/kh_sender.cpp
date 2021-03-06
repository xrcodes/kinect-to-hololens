#include <iostream>
#include <random>
#include <tuple>
#include "native/tt_native.h"
#include "sender/audio_sender.h"
#include "sender/video_pipeline.h"
#include "sender/video_sender_storage.h"
#include "sender/receiver_packet_classifier.h"
#include "win32/imgui_wrapper.h"
#include "utils/filesystem_utils.h"
#include "native/profiler.h"

namespace kh
{
constexpr int IMGUI_WIDTH{960};
constexpr int IMGUI_HEIGHT{540};
constexpr const char* INGUI_TITLE{"Kinect Sender"};

template<typename Func>
void imgui_loop(const Func& f)
{
    Win32Window window{init_imgui(IMGUI_WIDTH, IMGUI_HEIGHT, INGUI_TITLE)};

    // Main loop
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT) {
        // Poll and handle messages (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        f();
    }

    cleanup_imgui(window);
};

std::pair<bool, bool> plan_video_bitrate_control(std::map<int, RemoteReceiver>& remote_receivers, int last_frame_id, tt::TimePoint last_frame_time)
{
    bool video_required_by_any = false;
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (remote_receiver.video_requested) {
            video_required_by_any = true;
            break;
        }
    }
    if (!video_required_by_any)
        return {false, false};

    int min_receiver_frame_id{INT_MAX};
    for (auto& [_, remote_receiver] : remote_receivers) {
        // Receivers that did not request video are irrelevant.
        if (!remote_receiver.video_requested)
            continue;

        // Send out a keyframe if there is a new receiver.
        if (!remote_receiver.video_frame_id)
            return {true, true};

        if (*remote_receiver.video_frame_id < min_receiver_frame_id)
            min_receiver_frame_id = *remote_receiver.video_frame_id;
    }

    constexpr float AZURE_KINECT_FRAME_RATE{30.0f};
    const auto frame_time_point{tt::TimePoint::now()};
    const auto frame_time_diff{frame_time_point - last_frame_time};
    const int frame_id_diff{last_frame_id - min_receiver_frame_id};

    // Skip a frame if there is no new receiver that requires a frame to start
    // and the sender is too much ahead of the receivers.
    const bool is_ready{(frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) > std::pow(2, frame_id_diff - 1)};

    // Send a keyframe when there is a new receiver or at least a receiver needs to catch up by jumping forward using a keyframe.
    const bool keyframe{frame_id_diff > 5};

    return {is_ready, keyframe};
}

void send_video_message(VideoPipelineFrame& video_frame,
                        int sender_id,
                        tt::TimePoint session_start_time,
                        k4a::calibration calibration,
                        tt::UdpSocket& udp_socket,
                        VideoSenderStorage& video_sender_storage,
                        std::map<int, RemoteReceiver>& remote_receivers,
                        std::mt19937& rng)
{
    // Create video/parity packet bytes.
    const float video_frame_time_stamp{(video_frame.time_point - session_start_time).ms()};
    int width{calibration.depth_camera_calibration.resolution_width};
    int height{calibration.depth_camera_calibration.resolution_height};
    tt::KinectIntrinsics intrinsics;
    intrinsics.cx = calibration.depth_camera_calibration.intrinsics.parameters.param.cx;
    intrinsics.cy = calibration.depth_camera_calibration.intrinsics.parameters.param.cy;
    intrinsics.fx = calibration.depth_camera_calibration.intrinsics.parameters.param.fx;
    intrinsics.fy = calibration.depth_camera_calibration.intrinsics.parameters.param.fy;
    intrinsics.k1 = calibration.depth_camera_calibration.intrinsics.parameters.param.k1;
    intrinsics.k2 = calibration.depth_camera_calibration.intrinsics.parameters.param.k2;
    intrinsics.k3 = calibration.depth_camera_calibration.intrinsics.parameters.param.k3;
    intrinsics.k4 = calibration.depth_camera_calibration.intrinsics.parameters.param.k4;
    intrinsics.k5 = calibration.depth_camera_calibration.intrinsics.parameters.param.k5;
    intrinsics.k6 = calibration.depth_camera_calibration.intrinsics.parameters.param.k6;
    intrinsics.codx = calibration.depth_camera_calibration.intrinsics.parameters.param.codx;
    intrinsics.cody = calibration.depth_camera_calibration.intrinsics.parameters.param.cody;
    intrinsics.max_radius_for_projection = calibration.depth_camera_calibration.metric_radius;

    const auto message{tt::create_video_sender_message(video_frame_time_stamp, video_frame.keyframe, width, height, intrinsics,
                                                   video_frame.vp8_frame, video_frame.trvl_frame, video_frame.floor)};
    auto video_packets{tt::split_video_sender_message_bytes(sender_id, video_frame.frame_id, message.bytes)};
    auto parity_packets{tt::create_parity_sender_packets(sender_id, video_frame.frame_id, video_packets)};

    // Send video/parity packets.
    // Sending them in a random order makes the packets more robust to packet loss.
    std::vector<tt::Packet*> packet_ptrs;
    for (auto& video_packet : video_packets)
        packet_ptrs.push_back(&video_packet);

    for (auto& parity_packet : parity_packets)
        packet_ptrs.push_back(&parity_packet);

    std::shuffle(packet_ptrs.begin(), packet_ptrs.end(), rng);
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (!remote_receiver.video_requested)
            continue;

        for (auto& packet_bytes_ptr : packet_ptrs) {
            udp_socket.send(packet_bytes_ptr->bytes, remote_receiver.endpoint);
        }

        // Make video_frame_id no longer a std::nullopt so it won't get the
        // intialization privilege again.
        if (!remote_receiver.video_frame_id)
            remote_receiver.video_frame_id = video_frame.frame_id - 1;
    }

    // Save video/parity packet bytes for retransmission. 
    video_sender_storage.add(video_frame.frame_id, std::move(video_packets), std::move(parity_packets));
}

// Update receiver_state and summary with Report packets.
void apply_report_packets(std::vector<tt::ReportReceiverPacket>& report_packets,
                          RemoteReceiver& remote_receiver,
                          tt::Profiler& profiler)
{
    // Update receiver_state and summary with Report packets.
    for (auto& report_packet : report_packets) {
        if (!remote_receiver.video_frame_id) {
            remote_receiver.video_frame_id = report_packet.frame_id;
            continue;
        }

        // Ignore if network is somehow out of order and a report comes in out of order.
        if (report_packet.frame_id <= *remote_receiver.video_frame_id)
            continue;
        
        remote_receiver.video_frame_id = report_packet.frame_id;
        profiler.addNumber("report-count", 1);
    }
}

void retransmit_requested_packets(tt::UdpSocket& udp_socket,
                                  std::vector<tt::RequestReceiverPacket>& request_packets,
                                  VideoSenderStorage& video_sender_storage,
                                  const asio::ip::udp::endpoint remote_endpoint,
                                  tt::Profiler& profiler)
{
    if (request_packets.size() > 0)
        std::cout << "request_packets.size(): " << request_packets.size() << std::endl;

    // Retransmit the requested video packets.
    for (auto& request_packet : request_packets) {
        //std::cout << "received a request packet: " << request_packet.frame_id << std::endl;
        const int frame_id{request_packet.frame_id};

        auto video_frame_packets_it{video_sender_storage.find(frame_id)};
        if (video_frame_packets_it == video_sender_storage.end()) {
            //throw std::runtime_error("Could not find frame from VideoSenderStorage.");
            // A request packet can arrive after a report packet with a later frame_id.
            continue;
        }

        profiler.addNumber("retransmit-frame", 1);

        if (request_packet.all_packets) {
            for (auto& video_packet : video_frame_packets_it->second.video_packets) {
                udp_socket.send(video_packet.bytes, remote_endpoint);
                profiler.addNumber("retransmit-byte", video_packet.bytes.size());
            }

            for (auto& parity_packet : video_frame_packets_it->second.parity_packets) {
                udp_socket.send(parity_packet.bytes, remote_endpoint);
                profiler.addNumber("retransmit-byte", parity_packet.bytes.size());
            }

            profiler.addNumber("retransmit-video", video_frame_packets_it->second.video_packets.size());
            profiler.addNumber("retransmit-parity", video_frame_packets_it->second.parity_packets.size());
        } else {
            for (int packet_index : request_packet.video_packet_indices) {
                udp_socket.send(video_frame_packets_it->second.video_packets[packet_index].bytes, remote_endpoint);
                profiler.addNumber("retransmit-byte", video_frame_packets_it->second.video_packets[packet_index].bytes.size());
            }

            for (int packet_index : request_packet.parity_packet_indices) {
                udp_socket.send(video_frame_packets_it->second.parity_packets[packet_index].bytes, remote_endpoint);
                profiler.addNumber("retransmit-byte", video_frame_packets_it->second.parity_packets[packet_index].bytes.size());
            }

            profiler.addNumber("retransmit-video", request_packet.video_packet_indices.size());
            profiler.addNumber("retransmit-parity", request_packet.parity_packet_indices.size());
        }
    }
}

void log_receiver_report_summary(ExampleAppLog& log, tt::Profiler& profiler)
{
    log.AddLog("Receiver FPS %f\n", profiler.getNumber("report-count") / profiler.getElapsedTime().sec());
}

void log_video_pipeline_summary(ExampleAppLog& log, int last_frame_id, tt::Profiler& profiler)
{
    auto elapsed_time{profiler.getElapsedTime()};
    log.AddLog("VideoPipeline Summary:\n");
    log.AddLog("  Frame ID: %d\n", last_frame_id);
    log.AddLog("  FPS: %f\n", profiler.getNumber("pipeline-frame") / elapsed_time.sec());
    log.AddLog("  Color Bandwidth: %f Mbps\n", profiler.getNumber("pipeline-vp8byte") / elapsed_time.sec() / (1024.0f * 1024.0f / 8.0f));
    log.AddLog("  Depth Bandwidth: %f Mbps\n", profiler.getNumber("pipeline-trvlbyte") / elapsed_time.sec() / (1024.0f * 1024.0f / 8.0f));
    log.AddLog("  Keyframe Ratio: %f\n", profiler.getNumber("pipeline-keyframe") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Occlusion Removal Time Average: %f\n", profiler.getNumber("pipeline-occlusion") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Transformation Time Average: %f\n", profiler.getNumber("pipeline-mapping") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Yuv Conversion Time Average: %f\n", profiler.getNumber("pipeline-yuv") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Color Encoder Time Average: %f\n", profiler.getNumber("pipeline-vp8") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Depth Encoder Time Average: %f\n", profiler.getNumber("pipeline-trvl") / profiler.getNumber("pipeline-frame"));
    log.AddLog("  Floor Detection Time Average: %f\n", profiler.getNumber("pipeline-floor") / profiler.getNumber("pipeline-frame"));
}

void log_retransmission_summary(ExampleAppLog& log, tt::Profiler& profiler)
{
    auto elapsed_time{profiler.getElapsedTime()};
    log.AddLog("Retransmission Summary:\n");
    log.AddLog("  Frame Per Second: %f\n", profiler.getNumber("retransmit-frame") / elapsed_time.sec());
    log.AddLog("  Video Packet Per Second: %f\n", profiler.getNumber("retransmit-video") / elapsed_time.sec());
    log.AddLog("  Parity Packet Per Second: %f\n", profiler.getNumber("retransmit-parity") / elapsed_time.sec());
    log.AddLog("  Bandwidth: %f Mbps\n", profiler.getNumber("retransmit-byte") / elapsed_time.sec() / (1024.0f * 1024.0f / 8.0f));
}

void start(KinectInterface& kinect_interface)
{
    constexpr int DEFAULT_PORT{3773};
    constexpr int SENDER_SEND_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC{3.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{10.0f};
    constexpr float SUMMARY_INTERVAL_SEC{10.0f};

    // The default port (the port when nothing is entered) is 7777.
    const int sender_id{gsl::narrow<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};
    const k4a::calibration calibration{kinect_interface.getCalibration()};

    std::cout << "Start kinect_sender (sender_id: " << sender_id << ").\n";

    // Create UdpSocket.
    asio::io_context io_context;

    int port{DEFAULT_PORT};
    std::optional<asio::ip::udp::socket> socket{};
    for (int i = 0; i < 10; ++i) {
        try {
            socket = asio::ip::udp::socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
            break;
        } catch (std::system_error e) {
            // This can happen due to the port being occupied. Increment the port and try again.
            ++port;
        }
    }
    socket->set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});
    tt::UdpSocket udp_socket{std::move(*socket)};

    // Print IP addresses of this machine.
    asio::ip::udp::resolver resolver(io_context);
    asio::ip::udp::resolver::query query(asio::ip::host_name(), "");
    auto resolver_results = resolver.resolve(query);
    std::vector<std::string> local_addresses;
    std::cout << "local addresses:\n";
    for (auto it = resolver_results.begin(); it != resolver_results.end(); ++it) {
        if (it->endpoint().protocol() != asio::ip::udp::v4())
            continue;
        std::cout << "  - " << it->endpoint().address() << "\n";
        local_addresses.push_back(it->endpoint().address().to_string() + ":" + std::to_string(port));
    }

    // Initialize instances for loop below.
    const tt::TimePoint session_start_time{tt::TimePoint::now()};
    tt::TimePoint last_heartbeat_time{tt::TimePoint::now()};

    VideoPipeline video_pipeline{calibration};
    
    std::unique_ptr<AudioSender> audio_sender{nullptr};
    if (kinect_interface.isDevice())
        audio_sender.reset(new AudioSender(sender_id));
    
    VideoSenderStorage video_packet_storage;

    std::map<int, RemoteReceiver> remote_receivers;

    std::mt19937 rng{std::random_device{}()};

    tt::Profiler profiler;

    // Our state
    ExampleAppLog log;
    ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

    imgui_loop([&] {
        begin_imgui_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(IMGUI_WIDTH * 0.4f, IMGUI_HEIGHT * 0.4f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Sender Information");
        ImGui::Text("Sender ID: %d", sender_id);
        ImGui::Text("Frame ID: %d", video_pipeline.last_frame_id());
        ImGui::Text("IP End Points:");
        for (auto& address : local_addresses)
            ImGui::BulletText(address.c_str());
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, IMGUI_HEIGHT * 0.4f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(IMGUI_WIDTH * 0.4f, IMGUI_HEIGHT * 0.6f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Remote Receivers");
        for (auto& [_, remote_receiver] : remote_receivers) {
            ImGui::Text("End Point: %s:%d", remote_receiver.endpoint.address().to_string(), remote_receiver.endpoint.port());
            ImGui::BulletText("Receiver ID: %d", remote_receiver.receiver_id);
            ImGui::BulletText("Video: %s", remote_receiver.video_requested ? "Requested" : "Not Requested");
            ImGui::BulletText("Audio: %s", remote_receiver.audio_requested ? "Requested" : "Not Requested");
            ImGui::BulletText("Frame ID: %d", remote_receiver.video_frame_id.value_or(-1));
        }
        ImGui::End();

        // For the demo: add a debug button _BEFORE_ the normal log window contents
        // We take advantage of a rarely used feature: multiple calls to Begin()/End() are appending to the _same_ window.
        // Most of the contents of the window will be added by the log.Draw() call.
        ImGui::SetNextWindowPos(ImVec2(IMGUI_WIDTH * 0.4f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(IMGUI_WIDTH * 0.6f, IMGUI_HEIGHT), ImGuiCond_FirstUseEver);

        // Actually call in the regular Log helper (which will Begin() into the same window as we just did)
        log.Draw("Log");

        end_imgui_frame(clear_color);

        try {
            auto receiver_packet_collection{ReceiverPacketClassifier::classify(udp_socket, remote_receivers)};

            // Receive a connect packet from a receiver and capture the receiver's endpoint.
            // Then, create ReceiverState with it.
            for (auto& connect_packet_info : receiver_packet_collection.connect_packet_infos) {
                // Send packet confirming the receiver that the connect packet got received.
                udp_socket.send(tt::create_confirm_sender_packet(sender_id, connect_packet_info.connect_packet.receiver_id).bytes, connect_packet_info.receiver_endpoint);

                // Skip already existing receivers.
                if (remote_receivers.find(connect_packet_info.connect_packet.receiver_id) != remote_receivers.end())
                    continue;

                std::cout << "connect_packet_info.connect_packet_data.video_requested: " << connect_packet_info.connect_packet.video_requested << "\n";

                std::cout << "Receiver " << connect_packet_info.connect_packet.receiver_id << " connected.\n";
                remote_receivers.insert({connect_packet_info.connect_packet.receiver_id,
                                         RemoteReceiver{connect_packet_info.receiver_endpoint,
                                                        connect_packet_info.connect_packet.receiver_id,
                                                        connect_packet_info.connect_packet.video_requested,
                                                        connect_packet_info.connect_packet.audio_requested}});
            }

            // Skip the main part of the loop if there is no receiver connected.
            if (!remote_receivers.empty()) {
                // Send heartbeat packets to receivers.
                if (last_heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                    for (auto& [_, remote_receiver] : remote_receivers)
                        udp_socket.send(tt::create_heartbeat_sender_packet(sender_id).bytes, remote_receiver.endpoint);
                    last_heartbeat_time = tt::TimePoint::now();
                }

                // Send video packets to the receivers.
                auto [is_ready, keyframe] {plan_video_bitrate_control(remote_receivers, video_pipeline.last_frame_id(), video_pipeline.last_frame_time())};
                if (is_ready) {
                    // Try getting a Kinect frame.
                    auto kinect_frame{kinect_interface.getFrame()};
                    if (kinect_frame) {
                        auto video_frame{video_pipeline.process(*kinect_frame, keyframe, profiler)};
                        send_video_message(video_frame, sender_id, session_start_time, calibration,
                                           udp_socket, video_packet_storage, remote_receivers, rng);
                    }
                }

                // Send audio packets to the receivers.
                if (audio_sender)
                    audio_sender->send(udp_socket, remote_receivers);

                for (auto& [receiver_id, receiver_packet_set] : receiver_packet_collection.receiver_packet_infos) {
                    auto remote_receiver_ptr{&remote_receivers.at(receiver_id)};
                    if (receiver_packet_set.received_any) {
                        apply_report_packets(receiver_packet_set.report_packets,
                                             *remote_receiver_ptr,
                                             profiler);
                        retransmit_requested_packets(udp_socket,
                                                     receiver_packet_set.request_packets,
                                                     video_packet_storage,
                                                     remote_receiver_ptr->endpoint,
                                                     profiler);
                        remote_receiver_ptr->last_packet_time = tt::TimePoint::now();
                    } else {
                        if (remote_receiver_ptr->last_packet_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                            std::cout << "Timed out receiver " << receiver_id << " after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                            remote_receivers.erase(receiver_id);
                        }
                    }
                }
            }

            int min_receiver_frame_id{INT_MAX};
            for (auto& [_, remote_receiver] : remote_receivers) {
                if (remote_receiver.video_frame_id && *remote_receiver.video_frame_id < min_receiver_frame_id)
                    min_receiver_frame_id = *remote_receiver.video_frame_id;
            }

            if(min_receiver_frame_id != INT_MAX)
                video_packet_storage.cleanup(min_receiver_frame_id);

        } catch (tt::UdpSocketRuntimeError e) {
            std::cout << "UdpSocketRuntimeError\n  message: " << e.what() << "\n  endpoint: " << e.endpoint() << "\n";
            std::cout << "remote_receivers.size(): " << remote_receivers.size() << "\n";
            for (auto it{remote_receivers.begin()}; it != remote_receivers.end();) {
                if (it->second.endpoint == e.endpoint()) {
                    it = remote_receivers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        if (profiler.getElapsedTime().sec() > SUMMARY_INTERVAL_SEC) {
            log_receiver_report_summary(log, profiler);
            log_video_pipeline_summary(log, video_pipeline.last_frame_id(), profiler);
            log_retransmission_summary(log, profiler);
            profiler.reset();
        }
    });
}

void main()
{
    // First one is for running the application inside visual studio, and the other is for running the built application.
    const std::vector<std::string> DATA_FOLDER_PATHS{"../../../playback/", "../../../../playback/"};

    auto data_folder(find_data_folder(DATA_FOLDER_PATHS));

    if (data_folder) {
        std::cout << "Input filenames inside the data folder:" << std::endl;
        for (int i = 0; i < data_folder->filenames.size(); ++i) {
            std::cout << "    (" << i << ") " << data_folder->filenames[i] << std::endl;
        }

        std::cout << "Press Enter to Start with a Device or Enter Filename Index: ";
    } else {
        std::cout << "Failed to find the data folder...\n";

        std::cout << "Press Enter to Start: ";
    }

    std::string line;
    std::getline(std::cin, line);

    if (!data_folder || line == "") {
        try {
            KinectDevice kinect_device;
            kinect_device.start();
            start(kinect_device);
        } catch (k4a::error e) {
            std::cout << "Failed to create and start the Kinect device.\n";
        }
        return;
    }

    int filename_index;
    try {
        filename_index = stoi(line);
    } catch (std::invalid_argument) {
        std::cout << "invalid input: " << line << "\n";
        return;
    }

    std::cout << "filename_index: " << filename_index << std::endl;
    if (filename_index >= data_folder->filenames.size())
        std::cout << "filename_index out of range\n";

    auto filename{data_folder->filenames[filename_index]};
    std::cout << "filename: " << filename << std::endl;

    KinectPlayback playback{data_folder->folder_path + filename};
    start(playback);
}
}

int main()
{
    std::ios_base::sync_with_stdio(false);
    kh::main();

    std::cout << "\nPress Enter to Exit.\n";
    getchar();

    return 0;
}
