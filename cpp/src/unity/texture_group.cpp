#include "texture_group.h"

#include "interfaces/IUnityInterface.h"
#include "depth_texture.h"
#include "channel_texture.h"
#include "kh_trvl.h"

typedef void* VoidPtr;

// Color and depth texture sizes.
int width_;
int height_;

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
std::unique_ptr<kh::TrvlDecoder> depth_decoder_;
std::vector<short> depth_pixels_;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_reset()
{
    y_texture_ = nullptr;
    u_texture_ = nullptr;
    v_texture_ = nullptr;
    depth_texture_ = nullptr;

    y_texture_view_ = nullptr;
    u_texture_view_ = nullptr;
    v_texture_view_ = nullptr;
    depth_texture_view_ = nullptr;
}

// A function that intializes Direct3D resources. Should be called in a render thread.
void texture_group_init(ID3D11Device* device)
{
    y_texture_ = std::make_unique<kh::ChannelTexture>(device, width_, height_);
    u_texture_ = std::make_unique<kh::ChannelTexture>(device, width_ / 2, height_ / 2);
    v_texture_ = std::make_unique<kh::ChannelTexture>(device, width_ / 2, height_ / 2);
    depth_texture_ = std::make_unique<kh::DepthTexture>(device, width_, height_);

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

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_width()
{
    return width_;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_width(int width)
{
    width_ = width;
}

extern "C" int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_get_height()
{
    return height_;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_height(int height)
{
    height_ = height;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_ffmpeg_frame(void* ffmpeg_frame_ptr)
{
    auto ffmpeg_frame = reinterpret_cast<kh::FFmpegFrame*>(ffmpeg_frame_ptr);
    ffmpeg_frame_ = std::move(*ffmpeg_frame);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_pixels(void* depth_pixels_ptr)
{
    auto depth_pixels = reinterpret_cast<std::vector<short>*>(depth_pixels_ptr);
    depth_pixels_ = std::move(*depth_pixels);
}

// Updating pixels of the textures. Should be called in a render thread.
void texture_group_update(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    y_texture_->updatePixels(device, device_context, width_, height_, ffmpeg_frame_, 0);
    u_texture_->updatePixels(device, device_context, width_ / 2, height_ / 2, ffmpeg_frame_, 1);
    v_texture_->updatePixels(device, device_context, width_ / 2, height_ / 2, ffmpeg_frame_, 2);
    
    depth_texture_->updatePixels(device, device_context, width_, height_, reinterpret_cast<uint16_t*>(depth_pixels_.data()));
}