#include "texture_group.h"

#include "unity/IUnityInterface.h"
#include "depth_texture.h"
#include "channel_texture.h"
#include "kh_depth_compression_helper.h"

typedef void* VoidPtr;

// Color and depth texture sizes.
int color_width_;
int color_height_;
int depth_width_;
int depth_height_;

// Instances of classes for Direct3D textures.
std::unique_ptr<kh::ChannelTexture> y_texture_;
std::unique_ptr<kh::ChannelTexture> u_texture_;
std::unique_ptr<kh::ChannelTexture> v_texture_;
std::unique_ptr<kh::DepthTexture> depth_texture_;

// Unity connects Unity textures to Direct3D textures through creating Unity textures binded to these texture views.
ID3D11ShaderResourceView* y_texture_view_ = nullptr;
ID3D11ShaderResourceView* u_texture_view_ = nullptr;
ID3D11ShaderResourceView* v_texture_view_ = nullptr;
ID3D11ShaderResourceView* depth_texture_view_ = nullptr;

// These variables get set in the main thread of Unity, then gets assigned to textures in the render thread of Unity.
kh::FFmpegFrame ffmpeg_frame_(nullptr);
//std::vector<uint8_t> rvl_frame_;
std::unique_ptr<kh::DepthDecoder> depth_decoder_;
std::vector<uint8_t> depth_encoder_frame_;

// A function that intializes Direct3D resources. Should be called in a render thread.
void texture_group_init(ID3D11Device* device)
{
    y_texture_ = std::make_unique<kh::ChannelTexture>(device, color_width_, color_height_);
    u_texture_ = std::make_unique<kh::ChannelTexture>(device, color_width_ / 2, color_height_ / 2);
    v_texture_ = std::make_unique<kh::ChannelTexture>(device, color_width_ / 2, color_height_ / 2);
    depth_texture_ = std::make_unique<kh::DepthTexture>(device, depth_width_, depth_height_);

    // Set the texture view variables, so Unity can create Unity textures that are connected to the textures through the texture views.
    y_texture_view_ = y_texture_->getTextureView(device);
    u_texture_view_ = u_texture_->getTextureView(device);
    v_texture_view_ = v_texture_->getTextureView(device);
    depth_texture_view_ = depth_texture_->getTextureView(device);
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_y_texture_view()
{
    return y_texture_view_;
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_u_texture_view()
{
    return u_texture_view_;
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_v_texture_view()
{
    return v_texture_view_;
}

extern "C" VoidPtr UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_depth_texture_view()
{
    return depth_texture_view_;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_color_width(int color_width)
{
    color_width_ = color_width;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_color_height(int color_height)
{
    color_height_ = color_height;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_width(int depth_width)
{
    depth_width_ = depth_width;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_height(int depth_height)
{
    depth_height_ = depth_height;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_init_depth_encoder(int depth_compression_type)
{
    if (depth_compression_type == 0) {
        depth_decoder_ = std::make_unique<kh::RvlDepthDecoder>(depth_width_ * depth_height_);
    } else if (depth_compression_type == 1) {
        depth_decoder_ = std::make_unique<kh::TrvlDepthDecoder>(depth_width_ * depth_height_);
    } else if (depth_compression_type == 2) {
        depth_decoder_ = std::make_unique<kh::Vp8DepthDecoder>();
    }
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_ffmpeg_frame(void* ffmpeg_frame_ptr)
{
    auto ffmpeg_frame = reinterpret_cast<kh::FFmpegFrame*>(ffmpeg_frame_ptr);
    ffmpeg_frame_ = std::move(*ffmpeg_frame);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_encoder_frame(void* depth_encoder_frame_data, int depth_encoder_frame_size)
{
    depth_encoder_frame_ = std::vector<uint8_t>(depth_encoder_frame_size);
    memcpy(depth_encoder_frame_.data(), depth_encoder_frame_data, depth_encoder_frame_size);
}

// Updating pixels of the textures. Should be called in a render thread.
void texture_group_update(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    y_texture_->updatePixels(device, device_context, color_width_, color_height_, ffmpeg_frame_, 0);
    u_texture_->updatePixels(device, device_context, color_width_ / 2, color_height_ / 2, ffmpeg_frame_, 1);
    v_texture_->updatePixels(device, device_context, color_width_ / 2, color_height_ / 2, ffmpeg_frame_, 2);
    
    auto depth_pixels = depth_decoder_->decode(depth_encoder_frame_.data(), depth_encoder_frame_.size());
    depth_texture_->updatePixels(device, device_context, depth_width_, depth_height_, reinterpret_cast<uint16_t*>(depth_pixels.data()));
}