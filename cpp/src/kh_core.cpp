#include "kh_core.h"

#include "kh_rvl.h"

namespace kh
{
YuvImage createYuvImageFromAzureKinectYuy2Buffer(uint8_t* buffer, int width, int height, int stride)
{
    // Sizes assume Kinect runs in ColorImageFormat_Yuy2.
    std::vector<uint8_t> y_channel(width * height);
    std::vector<uint8_t> u_channel(width * height / 4);
    std::vector<uint8_t> v_channel(width * height / 4);

    // Conversion of the Y channels of the pixels.
    int y_channel_index = 0;
    for (int j = 0; j < height; ++j) {
        int buffer_index = j * stride;
        for (int i = 0; i < width; ++i) {
            y_channel[y_channel_index++] = buffer[buffer_index];
            buffer_index += 2;
        }
    }

    // Calculation of the U and V channels of the pixels.
    int uv_width = width / 2;
    int uv_height = height / 2;

    int uv_index = 0;
    for (int j = 0; j < uv_height; ++j) {
        int buffer_index = j * stride * 2 + 1;
        for (int i = 0; i < uv_width; ++i) {
            u_channel[uv_index] = buffer[buffer_index];
            v_channel[uv_index] = buffer[buffer_index + 2];
            ++uv_index;
            buffer_index += 4;
        }
    }

    return YuvImage(std::move(y_channel), std::move(u_channel), std::move(v_channel), width, height);
}

// A helper function for createYuvImageFromAvFrame that converts a AVFrame into a std::vector.
std::vector<uint8_t> convertPicturePlaneToBytes(uint8_t* data, int line_size, int width, int height)
{
    std::vector<uint8_t> bytes(width * height);
    for (int i = 0; i < height; ++i)
        memcpy(bytes.data() + i * width, data + i * line_size, width);

    return bytes;
}

// Converts an outcome of Vp8Deocder into YuvImage so it can be converted for OpenCV with createCvMatFromYuvImage().
YuvImage createYuvImageFromAvFrame(AVFrame* av_frame)
{
    return YuvImage(
        std::move(convertPicturePlaneToBytes(av_frame->data[0], av_frame->linesize[0], av_frame->width, av_frame->height)),
        std::move(convertPicturePlaneToBytes(av_frame->data[1], av_frame->linesize[1], av_frame->width / 2, av_frame->height / 2)),
        std::move(convertPicturePlaneToBytes(av_frame->data[2], av_frame->linesize[2], av_frame->width / 2, av_frame->height / 2)),
        av_frame->width,
        av_frame->height);
}
}