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

        std::optional<int> begin_frame_id;
        // If there is a key frame, use the most recent one.
        for (auto& frame_message_pair : video_messages) {
            if (frame_message_pair.first <= last_frame_id_)
                continue;

            if (frame_message_pair.second->keyframe)
                begin_frame_id = frame_message_pair.first;
        }

        // When there is no key frame, go through all the frames to check
        // if there is the one right after the previously rendered one.
        if (!begin_frame_id) {
            // If a frame message with frame_id == (last_frame_id + 1) is found
            if (video_messages.find(last_frame_id_ + 1) != video_messages.end()) {
                begin_frame_id = last_frame_id_ + 1;
            } else {
                // Wait for more frames if there is way to render without glitches.
                return;
            }
        }

        std::optional<tt::FFmpegFrame> ffmpeg_frame;
        std::vector<int16_t> depth_image;
        for (int i = *begin_frame_id; ; ++i) {
            // break loop is there is no frame with frame_id i.
            if (video_messages.find(i) == video_messages.end())
                break;

            const auto frame_message_pair_ptr{video_messages[i].get()};

            last_frame_id_ = i;

            // Decoding a Vp8Frame into color pixels.
            ffmpeg_frame = color_decoder_.decode(frame_message_pair_ptr->color_encoder_frame);
            // Decompressing a RVL frame into depth pixels.
            depth_image = depth_decoder_.decode(frame_message_pair_ptr->depth_encoder_frame, frame_message_pair_ptr->keyframe);
        }

        last_frame_time_ = tt::TimePoint::now();

        auto color_mat{create_cv_mat_from_yuv_image(tt::YuvFrame::create(*ffmpeg_frame))};
        auto depth_mat{create_cv_mat_from_kinect_depth_image(depth_image.data(), width_, height_)};

        // Rendering the depth pixels.
        cv::imshow("Color", color_mat);
        cv::imshow("Depth", depth_mat);
        if (cv::waitKey(1) >= 0)
            return;

        // Remove frame messages before the rendered frame.
        for (auto it = video_messages.begin(); it != video_messages.end();) {
            if (it->first < last_frame_id_) {
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