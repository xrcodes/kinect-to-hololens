#include <iostream>
#include <random>
#include <tuple>
#include "native/kh_native.h"
#include "sender/kinect_audio_sender.h"
#include "modules/video_pipeline.h"
#include "sender/video_sender_utils.h"
#include "sender/receiver_packet_receiver.h"
#include "native/imgui_wrapper.h"
#include "helper/filesystem_helper.h"
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

std::pair<bool, bool> plan_video_bitrate_control(std::unordered_map<int, RemoteReceiver>& remote_receivers, int last_frame_id, tt::TimePoint last_frame_time)
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

    int minimum_receiver_frame_id{INT_MAX};
    for (auto& [_, remote_receiver] : remote_receivers) {
        if (remote_receiver.video_frame_id < minimum_receiver_frame_id)
            minimum_receiver_frame_id = remote_receiver.video_frame_id;
    }

    // Send out a keyframe if there is a new receiver.
    if (minimum_receiver_frame_id == RemoteReceiver::INITIAL_VIDEO_FRAME_ID)
        return {true, true};

    constexpr float AZURE_KINECT_FRAME_RATE{30.0f};
    const auto frame_time_point{tt::TimePoint::now()};
    const auto frame_time_diff{frame_time_point - last_frame_time};
    const int frame_id_diff{last_frame_id - minimum_receiver_frame_id};

    // Skip a frame if there is no new receiver that requires a frame to start
    // and the sender is too much ahead of the receivers.
    const bool is_ready{(frame_time_diff.sec() * AZURE_KINECT_FRAME_RATE) > std::pow(2, frame_id_diff - 1)};

    // Send a keyframe when there is a new receiver or at least a receiver needs to catch up by jumping forward using a keyframe.
    const bool keyframe{frame_id_diff > 5};

    return {is_ready, keyframe};
}

void send_video_message(VideoPipelineFrame& video_frame,
                        int session_id,
                        tt::TimePoint session_start_time,
                        k4a::calibration calibration,
                        UdpSocket& udp_socket,
                        VideoPacketStorage& video_parity_packet_storage,
                        std::unordered_map<int, RemoteReceiver>& remote_receivers,
                        std::mt19937& rng)
{
    // Create video/parity packet bytes.
    const float video_frame_time_stamp{(video_frame.time_point - session_start_time).ms()};
    const auto message_bytes{create_video_sender_message_bytes(video_frame_time_stamp, video_frame.keyframe, calibration, video_frame.vp8_frame, video_frame.trvl_frame, video_frame.floor)};
    auto video_packets{split_video_sender_message_bytes(session_id, video_frame.frame_id, message_bytes)};
    auto parity_packets{create_parity_sender_packets(session_id, video_frame.frame_id, KH_FEC_PARITY_GROUP_SIZE, video_packets)};

    // Send video/parity packets.
    // Sending them in a random order makes the packets more robust to packet loss.
    std::vector<Packet*> packet_ptrs;
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
    }

    // Save video/parity packet bytes for retransmission. 
    video_parity_packet_storage.add(video_frame.frame_id, std::move(video_packets), std::move(parity_packets));
}

// Update receiver_state and summary with Report packets.
void apply_report_packets(std::vector<ReportReceiverPacketData>& report_packet_data_vector,
                          RemoteReceiver& remote_receiver,
                          Profiler& profiler)
{
    // Update receiver_state and summary with Report packets.
    for (auto& report_receiver_packet_data : report_packet_data_vector) {
        // Ignore if network is somehow out of order and a report comes in out of order.
        if (report_receiver_packet_data.frame_id <= remote_receiver.video_frame_id)
            continue;

        remote_receiver.video_frame_id = report_receiver_packet_data.frame_id;

        profiler.addNumber("report-decode", report_receiver_packet_data.decoder_time_ms);
        profiler.addNumber("report-interval", report_receiver_packet_data.frame_time_ms);
        profiler.addNumber("report-count", 1);
    }
}

void retransmit_requested_packets(UdpSocket& udp_socket,
                                  std::vector<RequestReceiverPacketData>& request_packet_data_vector,
                                  VideoPacketStorage& video_packet_storage,
                                  const asio::ip::udp::endpoint remote_endpoint)
{
    // Retransmit the requested video packets.
    for (auto& request_receiver_packet_data : request_packet_data_vector) {
        const int frame_id{request_receiver_packet_data.frame_id};
        if (!video_packet_storage.has(frame_id))
            continue;

        for (int packet_index : request_receiver_packet_data.video_packet_indices)
            udp_socket.send(video_packet_storage.get(frame_id).video_packets[packet_index].bytes, remote_endpoint);

        for (int packet_index : request_receiver_packet_data.parity_packet_indices)
            udp_socket.send(video_packet_storage.get(frame_id).parity_packets[packet_index].bytes, remote_endpoint);
    }
}

void log_receiver_report_summary(ExampleAppLog& log, Profiler& profiler)
{
    log.AddLog("Receiver Reported in %f Hz\n", profiler.getNumber("report-count") / profiler.getElapsedTime().sec());
    log.AddLog("  Decoder Time Average: %f ms\n", profiler.getNumber("report-decode") / profiler.getNumber("report-count"));
    log.AddLog("  Frame Interval Time Average: %f ms\n", profiler.getNumber("report-interval") / profiler.getNumber("report-count"));
}

void log_video_pipeline_summary(ExampleAppLog& log, int last_frame_id, Profiler& profiler)
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

void start(KinectInterface& kinect_interface)
{
    constexpr int DEFAULT_PORT{3773};
    constexpr int SENDER_SEND_BUFFER_SIZE{128 * 1024};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC{3.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{10.0f};
    constexpr float SUMMARY_INTERVAL_SEC{10.0f};

    // The default port (the port when nothing is entered) is 7777.
    const int session_id{gsl::narrow_cast<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};
    const k4a::calibration calibration{kinect_interface.getCalibration()};

    std::cout << "Start kinect_sender (session_id: " << session_id << ").\n";

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
    UdpSocket udp_socket{std::move(*socket)};

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
    
    std::unique_ptr<KinectAudioSender> kinect_audio_sender{nullptr};
    if (kinect_interface.isDevice())
        kinect_audio_sender.reset(new KinectAudioSender(session_id));
    
    VideoPacketStorage video_packet_storage;

    std::unordered_map<int, RemoteReceiver> remote_receivers;

    std::mt19937 rng{std::random_device{}()};

    Profiler profiler;

    // Our state
    ExampleAppLog log;
    ImVec4 clear_color{0.45f, 0.55f, 0.60f, 1.00f};

    imgui_loop([&] {
        begin_imgui_frame();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(IMGUI_WIDTH * 0.4f, IMGUI_HEIGHT * 0.4f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Local IP Addresses");
        for (auto& address : local_addresses)
            ImGui::BulletText(address.c_str());
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(0.0f, IMGUI_HEIGHT * 0.4f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(IMGUI_WIDTH * 0.4f, IMGUI_HEIGHT * 0.6f), ImGuiCond_FirstUseEver);
        ImGui::Begin("Remote Receivers");
        for (auto& [_, remote_receiver] : remote_receivers)
            ImGui::BulletText("Endpoint: %s:%d\nSession ID: %d\nVideo: %s\nAudio: %s",
                              remote_receiver.endpoint.address().to_string(),
                              remote_receiver.endpoint.port(),
                              remote_receiver.session_id,
                              remote_receiver.video_requested ? "Requested" : "Not Requested",
                              remote_receiver.audio_requested ? "Requested" : "Not Requested");
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
            auto receiver_packet_collection{ReceiverPacketReceiver::receive(udp_socket, remote_receivers)};

            // Receive a connect packet from a receiver and capture the receiver's endpoint.
            // Then, create ReceiverState with it.
            for (auto& connect_packet_info : receiver_packet_collection.connect_packet_infos) {
                // Send packet confirming the receiver that the connect packet got received.
                udp_socket.send(create_confirm_sender_packet_bytes(session_id, connect_packet_info.receiver_session_id), connect_packet_info.receiver_endpoint);

                // Skip already existing receivers.
                if (remote_receivers.find(connect_packet_info.receiver_session_id) != remote_receivers.end())
                    continue;

                std::cout << "connect_packet_info.connect_packet_data.video_requested: " << connect_packet_info.connect_packet_data.video_requested << "\n";

                std::cout << "Receiver " << connect_packet_info.receiver_session_id << " connected.\n";
                remote_receivers.insert({connect_packet_info.receiver_session_id,
                                         RemoteReceiver{connect_packet_info.receiver_endpoint,
                                                        connect_packet_info.receiver_session_id,
                                                        connect_packet_info.connect_packet_data.video_requested,
                                                        connect_packet_info.connect_packet_data.audio_requested}});
            }

            // Skip the main part of the loop if there is no receiver connected.
            if (!remote_receivers.empty()) {
                // Send heartbeat packets to receivers.
                if (last_heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                    for (auto& [_, remote_receiver] : remote_receivers)
                        udp_socket.send(create_heartbeat_sender_packet_bytes(session_id), remote_receiver.endpoint);
                    last_heartbeat_time = tt::TimePoint::now();
                }

                // Send video packets to the receivers.
                auto [is_ready, keyframe] {plan_video_bitrate_control(remote_receivers, video_pipeline.last_frame_id(), video_pipeline.last_frame_time())};
                if (is_ready) {
                    // Try getting a Kinect frame.
                    auto kinect_frame{kinect_interface.getFrame()};
                    if (kinect_frame) {
                        auto video_frame{video_pipeline.process(*kinect_frame, keyframe, profiler)};
                        send_video_message(video_frame, session_id, session_start_time, calibration,
                                           udp_socket, video_packet_storage, remote_receivers, rng);
                    }
                }

                // Send audio packets to the receivers.
                if (kinect_audio_sender)
                    kinect_audio_sender->send(udp_socket, remote_receivers);

                for (auto& [receiver_session_id, receiver_packet_set] : receiver_packet_collection.receiver_packet_sets) {
                    auto remote_receiver_ptr{&remote_receivers.at(receiver_session_id)};
                    if (receiver_packet_set.received_any) {
                        apply_report_packets(receiver_packet_set.report_packet_data_vector,
                                             *remote_receiver_ptr,
                                             profiler);
                        retransmit_requested_packets(udp_socket,
                                                     receiver_packet_set.request_packet_data_vector,
                                                     video_packet_storage,
                                                     remote_receiver_ptr->endpoint);
                        remote_receiver_ptr->last_packet_time = tt::TimePoint::now();
                    } else {
                        if (remote_receiver_ptr->last_packet_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                            std::cout << "Timed out receiver " << receiver_session_id << " after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                            remote_receivers.erase(receiver_session_id);
                        }
                    }
                }
            }

            video_packet_storage.cleanup(VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC);
        } catch (UdpSocketRuntimeError e) {
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
            profiler.reset();
        }
    });
}

void main()
{
    // First one is for running the application inside visual studio, and the other is for running the built application.
    const std::vector<std::string> DATA_FOLDER_PATHS{"../../../../playback/", "../../../../../playback/"};

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
