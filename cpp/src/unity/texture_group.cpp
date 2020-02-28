#include "texture_group.h"

#include "interfaces/IUnityInterface.h"

std::unique_ptr<TextureGroup> texture_group_;

// A function that intializes Direct3D resources. Should be called in a render thread.
void texture_group_init(ID3D11Device* device)
{
    texture_group_->y_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_->width, texture_group_->height);
    texture_group_->u_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_->width / 2, texture_group_->height / 2);
    texture_group_->v_texture = std::make_unique<kh::ChannelTexture>(device, texture_group_->width / 2, texture_group_->height / 2);
    texture_group_->depth_texture = std::make_unique<kh::DepthTexture>(device, texture_group_->width, texture_group_->height);

    // Set the texture view variables, so Unity can create Unity textures that are connected to the textures through the texture views.
    texture_group_->y_texture_view = texture_group_->y_texture->getTextureView(device);
    texture_group_->u_texture_view = texture_group_->u_texture->getTextureView(device);
    texture_group_->v_texture_view = texture_group_->v_texture->getTextureView(device);
    texture_group_->depth_texture_view = texture_group_->depth_texture->getTextureView(device);
}

// Updating pixels of the textures. Should be called in a render thread.
void texture_group_update(ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    texture_group_->y_texture->updatePixels(device, device_context, texture_group_->width, texture_group_->height, texture_group_->ffmpeg_frame, 0);
    texture_group_->u_texture->updatePixels(device, device_context, texture_group_->width / 2, texture_group_->height / 2, texture_group_->ffmpeg_frame, 1);
    texture_group_->v_texture->updatePixels(device, device_context, texture_group_->width / 2, texture_group_->height / 2, texture_group_->ffmpeg_frame, 2);

    texture_group_->depth_texture->updatePixels(device, device_context, texture_group_->width, texture_group_->height, reinterpret_cast<uint16_t*>(texture_group_->depth_pixels.data()));
}

extern "C"
{
    UNITY_INTERFACE_EXPORT TextureGroup* UNITY_INTERFACE_API texture_group_reset()
    {
        texture_group_ = std::make_unique<TextureGroup>();
        return texture_group_.get();
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_y_texture_view(TextureGroup* texture_group)
    {
        return texture_group->y_texture_view;
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_u_texture_view(TextureGroup* texture_group)
    {
        return texture_group->u_texture_view;
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_v_texture_view(TextureGroup* texture_group)
    {
        return texture_group->v_texture_view;
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_depth_texture_view(TextureGroup* texture_group)
    {
        return texture_group->depth_texture_view;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API texture_group_get_width(TextureGroup* texture_group)
    {
        return texture_group->width;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_width(TextureGroup* texture_group, int width)
    {
        texture_group->width = width;
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API texture_group_get_height(TextureGroup* texture_group)
    {
        return texture_group->height;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_height(TextureGroup* texture_group, int height)
    {
        texture_group->height = height;
    }

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_ffmpeg_frame(TextureGroup* texture_group, kh::FFmpegFrame* ffmpeg_frame)
    {
        texture_group->ffmpeg_frame = std::move(*ffmpeg_frame);
    }

    void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_pixels(TextureGroup* texture_group, std::vector<short>* depth_pixels)
    {
        texture_group->depth_pixels = std::move(*depth_pixels);
    }
}