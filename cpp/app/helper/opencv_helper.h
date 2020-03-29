#pragma once

#include <opencv2/opencv.hpp>
#include "kh_yuv.h"

namespace kh
{
cv::Mat create_cv_mat_from_yuv_image(const YuvImage& yuv_image)
{
    cv::Mat y_channel(yuv_image.height(), yuv_image.width(), CV_8UC1, const_cast<std::uint8_t*>(yuv_image.y_channel().data()));
    cv::Mat u_channel(yuv_image.height() / 2, yuv_image.width() / 2, CV_8UC1, const_cast<std::uint8_t*>(yuv_image.u_channel().data()));
    cv::Mat v_channel(yuv_image.height() / 2, yuv_image.width() / 2, CV_8UC1, const_cast<std::uint8_t*>(yuv_image.v_channel().data()));
    cv::Mat cr_channel;
    cv::Mat cb_channel;
    // u and v corresponds to Cb and Cr
    cv::resize(v_channel, cr_channel, cv::Size(v_channel.cols * 2, v_channel.rows * 2));
    cv::resize(u_channel, cb_channel, cv::Size(u_channel.cols * 2, u_channel.rows * 2));

    std::vector<cv::Mat> y_cr_cb_channels;
    y_cr_cb_channels.push_back(y_channel);
    y_cr_cb_channels.push_back(cr_channel);
    y_cr_cb_channels.push_back(cb_channel);

    cv::Mat y_cr_cb_frame;
    cv::merge(y_cr_cb_channels, y_cr_cb_frame);

    cv::Mat bgr_frame = y_cr_cb_frame.clone();
    cvtColor(y_cr_cb_frame, bgr_frame, cv::COLOR_YCrCb2BGR);
    return bgr_frame;
}

cv::Mat create_cv_mat_from_kinect_depth_image(const int16_t* depth_buffer, int width, int height)
{
    std::vector<uint8_t> reduced_depth_frame(width * height);
    std::vector<uint8_t> half(width * height);

    for (int i = 0; i < width * height; ++i) {
        reduced_depth_frame[i] = depth_buffer[i] / 32;
        half[i] = 128;
    }

    cv::Mat y_channel(height, width, CV_8UC1, reduced_depth_frame.data());
    cv::Mat cr_channel(height, width, CV_8UC1, half.data());
    cv::Mat cb_channel(height, width, CV_8UC1, half.data());

    std::vector<cv::Mat> y_cr_cb_channels;
    y_cr_cb_channels.push_back(y_channel);
    y_cr_cb_channels.push_back(cr_channel);
    y_cr_cb_channels.push_back(cb_channel);

    cv::Mat y_cr_cb_frame;
    cv::merge(y_cr_cb_channels, y_cr_cb_frame);

    cv::Mat bgr_frame = y_cr_cb_frame.clone();
    cvtColor(y_cr_cb_frame, bgr_frame, cv::COLOR_YCrCb2BGR);
    return bgr_frame;
}
}