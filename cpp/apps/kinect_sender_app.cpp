#include <iostream>
#include <random>
#include <tuple>
#include "native/kh_native.h"
#include "sender/kinect_audio_sender.h"
#include "sender/kinect_video_sender.h"
#include "sender/receiver_packet_receiver.h"
#include "helper/imgui_helper.h"
#include "helper/filesystem_helper.h"

namespace kh
{
// Update receiver_state and summary with Report packets.
void apply_report_packets(std::vector<ReportReceiverPacketData>& report_packet_data_vector,
                          RemoteReceiver& remote_receiver,
                          ReceiverReportSummary& summary)
{
    // Update receiver_state and summary with Report packets.
    for (auto& report_receiver_packet_data : report_packet_data_vector) {
        // Ignore if network is somehow out of order and a report comes in out of order.
        if (report_receiver_packet_data.frame_id <= remote_receiver.video_frame_id)
            continue;

        remote_receiver.video_frame_id = report_receiver_packet_data.frame_id;

        summary.decoder_time_ms_sum += report_receiver_packet_data.decoder_time_ms;
        summary.frame_interval_ms_sum += report_receiver_packet_data.frame_time_ms;
        ++summary.received_report_count;
    }
}

void retransmit_requested_packets(UdpSocket& udp_socket,
                                  std::vector<RequestReceiverPacketData>& request_packet_data_vector,
                                  VideoParityPacketStorage& video_parity_packet_storage,
                                  const asio::ip::udp::endpoint remote_endpoint)
{
    // Retransmit the requested video packets.
    for (auto& request_receiver_packet_data : request_packet_data_vector) {
        const int frame_id{request_receiver_packet_data.frame_id};
        if (!video_parity_packet_storage.has(frame_id))
            continue;

        for (int packet_index : request_receiver_packet_data.video_packet_indices)
            udp_socket.send(video_parity_packet_storage.get(frame_id).video_packet_byte_set[packet_index], remote_endpoint);

        for (int packet_index : request_receiver_packet_data.parity_packet_indices)
            udp_socket.send(video_parity_packet_storage.get(frame_id).parity_packet_byte_set[packet_index], remote_endpoint);
    }
}

void log_receiver_report_summary(ExampleAppLog& log, ReceiverReportSummary summary, TimeDuration duration)
{
    log.AddLog("Receiver Reported in %f Hz\n", summary.received_report_count / duration.sec());
    log.AddLog("  Decoder Time Average: %f ms\n", summary.decoder_time_ms_sum / summary.received_report_count);
    log.AddLog("  Frame Interval Time Average: %f ms\n", summary.frame_interval_ms_sum / summary.received_report_count);
}

void log_kinect_video_sender_summary(ExampleAppLog& log, KinectVideoSenderSummary summary, TimeDuration duration)
{
    log.AddLog("KinectDeviceManager Summary:\n");
    log.AddLog("  Frame ID: %d\n", summary.frame_id);
    log.AddLog("  FPS: %f\n", summary.frame_count / duration.sec());
    log.AddLog("  Color Bandwidth: %f Mbps\n", summary.color_byte_count / duration.sec() / (1024.0f * 1024.0f / 8.0f));
    log.AddLog("  Depth Bandwidth: %f Mbps\n", summary.depth_byte_count / duration.sec() / (1024.0f * 1024.0f / 8.0f));
    log.AddLog("  Keyframe Ratio: %f\n", static_cast<float>(summary.keyframe_count) / summary.frame_count);
    log.AddLog("  Occlusion Removal Time Average: %f\n", summary.occlusion_removal_ms_sum / summary.frame_count);
    log.AddLog("  Transformation Time Average: %f\n", summary.transformation_ms_sum / summary.frame_count);
    log.AddLog("  Yuv Conversion Time Average: %f\n", summary.yuv_conversion_ms_sum / summary.frame_count);
    log.AddLog("  Color Encoder Time Average: %f\n", summary.color_encoder_ms_sum / summary.frame_count);
    log.AddLog("  Depth Encoder Time Average: %f\n", summary.depth_encoder_ms_sum / summary.frame_count);
}

void start(KinectDeviceInterface& kinect_interface)
{
    constexpr int PORT{3773};
    constexpr int SENDER_SEND_BUFFER_SIZE{128 * 1024};
    constexpr int IMGUI_WIDTH{960};
    constexpr int IMGUI_HEIGHT{540};
    constexpr const char* INGUI_TITLE{"Kinect Sender"};
    constexpr float HEARTBEAT_INTERVAL_SEC{1.0f};
    constexpr float VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC{3.0f};
    constexpr float HEARTBEAT_TIME_OUT_SEC{10.0f};
    constexpr float SUMMARY_INTERVAL_SEC{10.0f};

    // The default port (the port when nothing is entered) is 7777.
    const int session_id{gsl::narrow_cast<const int>(std::random_device{}() % (static_cast<unsigned int>(INT_MAX) + 1))};

    std::cout << "Start kinect_sender (session_id: " << session_id << ").\n";

    // Create UdpSocket.
    asio::io_context io_context;
    asio::ip::udp::socket socket{io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), PORT)};
    socket.set_option(asio::socket_base::send_buffer_size{SENDER_SEND_BUFFER_SIZE});
    UdpSocket udp_socket{std::move(socket)};

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
        local_addresses.push_back(it->endpoint().address().to_string());
    }

    // Initialize instances for loop below.
    const TimePoint session_start_time{TimePoint::now()};
    TimePoint heartbeat_time{TimePoint::now()};

    KinectVideoSender kinect_video_sender{session_id, kinect_interface};
    KinectVideoSenderSummary kinect_video_sender_summary;

    KinectAudioSender kinect_audio_sender{session_id};
    
    ReceiverReportSummary receiver_report_summary;

    VideoParityPacketStorage video_parity_packet_storage;

    std::unordered_map<int, RemoteReceiver> remote_receivers;

    Win32Window window{init_imgui(IMGUI_WIDTH, IMGUI_HEIGHT, INGUI_TITLE)};

    // Our state
    ExampleAppLog log;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

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
            std::vector<int> receiver_session_ids;
            for (auto& [receiver_session_id, _] : remote_receivers)
                receiver_session_ids.push_back(receiver_session_id);
            
            auto receiver_packet_collection{ReceiverPacketReceiver::receive(udp_socket, receiver_session_ids)};

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
                std::vector<asio::ip::udp::endpoint> remote_endpoints;
                for (auto& [_, remote_receiver] : remote_receivers)
                    remote_endpoints.push_back(remote_receiver.endpoint);

                // Send video/audio packets to the receivers.
                kinect_video_sender.send(session_start_time, udp_socket, kinect_interface, video_parity_packet_storage, remote_receivers, kinect_video_sender_summary);
                kinect_audio_sender.send(udp_socket, remote_receivers);

                // Send heartbeat packets to receivers.
                if (heartbeat_time.elapsed_time().sec() > HEARTBEAT_INTERVAL_SEC) {
                    for (auto& [_, remote_receiver] : remote_receivers)
                        udp_socket.send(create_heartbeat_sender_packet_bytes(session_id), remote_receiver.endpoint);
                    heartbeat_time = TimePoint::now();
                }

                for (auto& [receiver_session_id, receiver_packet_set] : receiver_packet_collection.receiver_packet_sets) {
                    auto remote_receiver_ptr{&remote_receivers.at(receiver_session_id)};
                    if (receiver_packet_set.received_any) {
                        apply_report_packets(receiver_packet_set.report_packet_data_vector,
                                             *remote_receiver_ptr,
                                             receiver_report_summary);
                        retransmit_requested_packets(udp_socket,
                                                     receiver_packet_set.request_packet_data_vector,
                                                     video_parity_packet_storage,
                                                     remote_receiver_ptr->endpoint);
                        remote_receiver_ptr->last_packet_time = TimePoint::now();
                    } else {
                        if (remote_receiver_ptr->last_packet_time.elapsed_time().sec() > HEARTBEAT_TIME_OUT_SEC) {
                            std::cout << "Timed out receiver " << receiver_session_id << " after waiting for " << HEARTBEAT_TIME_OUT_SEC << " seconds without a received packet.\n";
                            remote_receivers.erase(receiver_session_id);
                        }
                    }
                }
            }

            video_parity_packet_storage.cleanup(VIDEO_PARITY_PACKET_STORAGE_TIME_OUT_SEC);
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

        const auto summary_duration{receiver_report_summary.time_point.elapsed_time()};
        if (summary_duration.sec() > SUMMARY_INTERVAL_SEC) {
            log_receiver_report_summary(log, receiver_report_summary, summary_duration);
            receiver_report_summary = ReceiverReportSummary{};

            log_kinect_video_sender_summary(log, kinect_video_sender_summary, summary_duration);
            kinect_video_sender_summary = KinectVideoSenderSummary{};
        }
    }

    cleanup_imgui(window);
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
