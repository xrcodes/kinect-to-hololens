#include "texture_group.h"

#include "interfaces/IUnityInterface.h"
#include "depth_texture.h"
#include "channel_texture.h"
#include "kh_trvl.h"

struct TextureGroup
{
    // Color and depth texture sizes.
    int width;
    int height;

    // Instances of classes for Direct3D textures.
    std::unique_ptr<kh::ChannelTexture> y_texture{nullptr};
    std::unique_ptr<kh::ChannelTexture> u_texture{nullptr};
    std::unique_ptr<kh::ChannelTexture> v_texture{nullptr};
    std::unique_ptr<kh::DepthTexture> depth_texture{nullptr};

    // Unity connects Unity textures to Direct3D textures through creating Unity textures binded to these texture views.
    ID3D11ShaderResourceView* y_texture_view{nullptr};
    ID3D11ShaderResourceView* u_texture_view{nullptr};
    ID3D11ShaderResourceView* v_texture_view{nullptr};
    ID3D11ShaderResourceView* depth_texture_view{nullptr};

    // These variables get set in the main thread of Unity, then gets assigned to textures in the render thread of Unity.
    kh::FFmpegFrame ffmpeg_frame{nullptr};
    std::unique_ptr<kh::TrvlDecoder> depth_decoder;
    std::vector<short> depth_pixels;
};

//// Color and depth texture sizes.
//int width_;
//int height_;
//
//// Instances of classes for Direct3D textures.
//std::unique_ptr<kh::ChannelTexture> y_texture_;
//std::unique_ptr<kh::ChannelTexture> u_texture_;
//std::unique_ptr<kh::ChannelTexture> v_texture_;
//std::unique_ptr<kh::DepthTexture> depth_texture_;
//
//// Unity connects Unity textures to Direct3D textures through creating Unity textures binded to these texture views.
//ID3D11ShaderResourceView* y_texture_view_ = nullptr;
//ID3D11ShaderResourceView* u_texture_view_ = nullptr;
//ID3D11ShaderResourceView* v_texture_view_ = nullptr;
//ID3D11ShaderResourceView* depth_texture_view_ = nullptr;
//
//// These variables get set in the main thread of Unity, then gets assigned to textures in the render thread of Unity.
//kh::FFmpegFrame ffmpeg_frame_(nullptr);
//std::unique_ptr<kh::TrvlDecoder> depth_decoder_;
//std::vector<short> depth_pixels_;

TextureGroup texture_group_;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_reset()
{
    //y_texture_ = nullptr;
    //u_texture_ = nullptr;
    //v_texture_ = nullptr;
    //depth_texture_ = nullptr;

    //y_texture_view_ = nullptr;
    //u_texture_view_ = nullptr;
    //v_texture_view_ = nullptr;
    //depth_texture_view_ = nullptr;
    texture_group_ = TextureGroup{};
}

// A function that intializes Direct3D resources. Should be called in a render thread.
void texture_group_init(ID3D11Device* device)
{
    //y_texture_ = std::make_unique<kh::ChannelTexture>(device, width_, height_);
    //u_texture_ = std::make_unique<kh::ChannelTexture>(device, width_ / 2, height_ / 2);
    //v_texture_ = std::make_unique<kh::ChannelTexture>(device, width_ / 2, height_ / 2);
    //depth_texture_ = std::make_unique<kh::DepthTexture>(device, width_, height_);

    //// Set the texture view variables, so Unity can create Unity textures that are connected to the textures through the texture views.
    //y_texture_view_ = y_texture_->getTextureView(device);
    //u_texture_view_ = u_texture_->getTextureView(device);
    //v_texture_view_ = v_texture_->getTextureView(device);
    //depth_texture_view_ = depth_texture_->getTextureView(device);
    texture_group_.y_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_.width, texture_group_.height);
    texture_group_.u_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_.width / 2, texture_group_.height / 2);
    texture_group_.v_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_.width / 2, texture_group_.height / 2);
    texture_group_.depth_texture = std::make_unique<kh::DepthTexture>(device, texture_group_.width, texture_group_.height);

    // Set the texture view variables, so Unity can create Unity textures that are connected to the textures through the texture views.
    texture_group_.y_texture_view = texture_group_.y_texture->getTextureView(device);
    texture_group_.u_texture_view = texture_group_.u_texture->getTextureView(device);
    texture_group_.v_texture_view = texture_group_.v_texture->getTextureView(device);
    texture_group_.depth_texture_view = texture_group_.depth_texture->getTextureView(device);
}

extern "C" UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_y_texture_view()
{
    return texture_group_.y_texture_view;
}

extern "C" UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_u_texture_view()
{
    return texture_group_.u_texture_view;
}

extern "C" UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_v_texture_view()
{
    return texture_group_.v_texture_view;
}

extern "C" UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_depth_texture_view()
{
    return texture_group_.depth_texture_view;
}

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API texture_group_get_width()
{
    return texture_group_.width;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_width(int width)
{
    texture_group_.width = width;
}

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API texture_group_get_height()
{
    return texture_group_.height;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_height(int height)
{
    texture_group_.height = height;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_ffmpeg_frame(void* ffmpeg_frame_ptr)
{
    auto ffmpeg_frame = reinterpret_cast<kh::FFmpegFrame*>(ffmpeg_frame_ptr);
    texture_group_.ffmpeg_frame = std::move(*ffmpeg_frame);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_pixels(void* depth_pixels_ptr)
{
    auto depth_pixels = reinterpret_cast<std::vector<short>*>(depth_pixels_ptr);
    texture_group_.depth_pixels = std::move(*depth_pixels);
}

// Updating pixels of the textures. Should be called in a render thread.
void texture_group_update(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    texture_group_.y_texture->updatePixels(device, device_context, texture_group_.width, texture_group_.height, texture_group_.ffmpeg_frame, 0);
    texture_group_.u_texture->updatePixels(device, device_context, texture_group_.width / 2, texture_group_.height / 2, texture_group_.ffmpeg_frame, 1);
    texture_group_.v_texture->updatePixels(device, device_context, texture_group_.width / 2, texture_group_.height / 2, texture_group_.ffmpeg_frame, 2);
    
    texture_group_.depth_texture->updatePixels(device, device_context, texture_group_.width, texture_group_.height, reinterpret_cast<uint16_t*>(texture_group_.depth_pixels.data()));
}