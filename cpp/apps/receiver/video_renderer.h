#pragma once

#include "video_renderer_state.h"

namespace kh
{
class VideoRenderer
{
public:
    VideoRenderer(const int session_id, const asio::ip::udp::endpoint remote_endpoint, int width, int height)
        : session_id_{session_id}, remote_endpoint_{remote_endpoint}, width_{width}, height_{height},
        color_decoder_{}, depth_decoder_{width * height}
    {
    }

    void render(UdpSocket& udp_socket,
                VideoRendererState& video_renderer_state,
                std::map<int, VideoSenderMessageData>& video_frame_messages)
    {
        if (video_frame_messages.empty())
            return;

        std::optional<int> begin_frame_id;
        // If there is a key frame, use the most recent one.
        for (auto& frame_message_pair : video_frame_messages) {
            if (frame_message_pair.first <= video_renderer_state.frame_id)
                continue;

            if (frame_message_pair.second.keyframe)
                begin_frame_id = frame_message_pair.first;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!begin_frame_id) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            if (video_frame_messages.find(video_renderer_state.frame_id + 1) != video_frame_messages.end()) {
                begin_frame_id = video_renderer_state.frame_id + 1;
            } else {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        std::optional<tt::FFmpegFrame> ffmpeg_frame;
        std::vector<short> depth_image;
        const auto decoder_start{tt::TimePoint::now()};
        for (int i = *begin_frame_id; ; ++i) {
            // break loop is there is no frame with frame_id i.
            if (video_frame_messages.find(i) == video_frame_messages.end())
                break;

            const auto frame_message_pair_ptr{&video_frame_messages[i]};

            video_renderer_state.frame_id = i;

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder_.decode(frame_message_pair_ptr->color_encoder_frame);
            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder_.decode(frame_message_pair_ptr->depth_encoder_frame, frame_message_pair_ptr->keyframe);
        }

        udp_socket.send(create_report_receiver_packet_bytes(session_id_,
                                                            video_renderer_state.frame_id,
                                                            decoder_start.elapsed_time().ms(),
                                                            video_renderer_state.last_frame_time_point.elapsed_time().ms()), remote_endpoint_);
        video_renderer_state.last_frame_time_point = tt::TimePoint::now();

        auto color_mat{create_cv_mat_from_yuv_image(tt::YuvFrame::create(*ffmpeg_frame))};
        auto depth_mat{create_cv_mat_from_kinect_depth_image(depth_image.data(), width_, height_)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            return;

        // Remove frame messages before the rendered frame.
        for (auto it = video_frame_messages.begin(); it != video_frame_messages.end();) {
            if (it->first < video_renderer_state.frame_id) {
                it = video_frame_messages.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    const int session_id_;
    const asio::ip::udp::endpoint remote_endpoint_;
    int width_;
    int height_;
    tt::Vp8Decoder color_decoder_;
    tt::TrvlDecoder depth_decoder_;
};
}