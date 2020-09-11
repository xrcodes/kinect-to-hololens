#pragma once

namespace kh
{
class VideoRenderer
{
public:
    VideoRenderer(const int receiver_id, const asio::ip::udp::endpoint remote_endpoint, int width, int height)
        : width_{width}, height_{height}, color_decoder_{}, depth_decoder_{width * height}, last_frame_id_{-1}, last_frame_time_{tt::TimePoint::now()}
    {
    }

    int last_frame_id() { return last_frame_id_; }

    void render(UdpSocket& udp_socket, std::map<int, std::shared_ptr<VideoSenderMessage>>& video_messages)
    {
        if (video_messages.empty())
            return;

        VideoSenderMessage* video_message_ptr{nullptr};
        // If there is a key frame, use the most recent one.
        for (auto& [frame_id, video_message] : video_messages) {
            if (frame_id <= last_frame_id_)
                continue;

            if (video_message->keyframe) {
                video_message_ptr = video_message.get();
                last_frame_id_ = frame_id;
            }
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!video_message_ptr) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            auto video_message_it{video_messages.find(last_frame_id_ + 1)};
            if (video_message_it != video_messages.end()) {
                video_message_ptr = video_message_it->second.get();
                last_frame_id_ = video_message_it->first;
            } else {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        std::optional<tt::FFmpegFrame> ffmpeg_frame{color_decoder_.decode(video_message_ptr->color_encoder_frame)};
        std::vector<int16_t> trvl_frame{depth_decoder_.decode(video_message_ptr->depth_encoder_frame, video_message_ptr->keyframe)};

        last_frame_time_ = tt::TimePoint::now();

        auto color_mat{create_cv_mat_from_yuv_image(tt::YuvFrame::create(*ffmpeg_frame))};
        auto depth_mat{create_cv_mat_from_kinect_depth_image(trvl_frame.data(), width_, height_)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            return;

        // Remove frame messages before and including the rendered frame.
        for (auto it = video_messages.begin(); it != video_messages.end();) {
            if (it->first <= last_frame_id_) {
                it = video_messages.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    int width_;
    int height_;
    tt::Vp8Decoder color_decoder_;
    tt::TrvlDecoder depth_decoder_;
    int last_frame_id_;
    tt::TimePoint last_frame_time_;
};
}