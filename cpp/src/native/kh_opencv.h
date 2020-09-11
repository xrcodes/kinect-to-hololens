#pragma once

#include <opencv2/opencv.hpp>
#include "core/tt_core.h"

namespace kh
{
cv::Mat create_cv_mat_from_kinect_color_image(uint8_t* color_buffer, int width, int height);
cv::Mat create_cv_mat_from_yuv_image(const tt::YuvFrame& yuv_image);
cv::Mat create_cv_mat_from_kinect_depth_image(const int16_t* depth_buffer, int width, int height);
}