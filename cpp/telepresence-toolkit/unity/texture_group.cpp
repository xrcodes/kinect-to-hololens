#include "texture_group.h"

#include "external/IUnityInterface.h"

std::unordered_map<int, std::unique_ptr<TextureGroup>> texture_groups_;
int next_texture_group_id_;

// A function that intializes Direct3D resources. Should be called in a render thread.
void texture_group_init(int texture_group_id, ID3D11Device* device)
{
    TextureGroup* texture_group{texture_groups_.at(texture_group_id).get()};
    texture_group->y_texture = std::make_unique<tt::ChannelTexture>(device, texture_group->width, texture_group->height);
    texture_group->uv_texture = std::make_unique<tt::TwoChannelTexture>(device, texture_group->width / 2, texture_group->height / 2);
    texture_group->depth_texture = std::make_unique<tt::DepthTexture>(device, texture_group->width, texture_group->height);

    // Set the texture view variables, so Unity can create Unity textures that are connected to the textures through the texture views.
    texture_group->y_texture_view = texture_group->y_texture->getTextureView(device);
    texture_group->uv_texture_view = texture_group->uv_texture->getTextureView(device);
    texture_group->depth_texture_view = texture_group->depth_texture->getTextureView(device);
}

// Updating pixels of the textures. Should be called in a render thread.
void texture_group_update(int texture_group_id, ID3D11Device* device, ID3D11DeviceContext* device_context)
{
    TextureGroup* texture_group{texture_groups_.at(texture_group_id).get()};
    texture_group->y_texture->updatePixels(device_context,
                                           texture_group->av_frame->data[0],
                                           texture_group->av_frame->linesize[0]);
    texture_group->uv_texture->updatePixels(device_context,
                                            texture_group->av_frame->data[1],
                                            texture_group->av_frame->linesize[1],
                                            texture_group->av_frame->data[2],
                                            texture_group->av_frame->linesize[2]);

    texture_group->depth_texture->updatePixels(device, device_context, texture_group->width, texture_group->height, texture_group->depth_pixels.data());
}

extern "C"
{
    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_reset()
    {
        texture_groups_ = std::unordered_map<int, std::unique_ptr<TextureGroup>>();
        next_texture_group_id_ = 0;
    }

    UNITY_INTERFACE_EXPORT TextureGroup* UNITY_INTERFACE_API texture_group_create()
    {
        int texture_group_id{next_texture_group_id_++};
        texture_groups_.insert({texture_group_id, std::make_unique<TextureGroup>(texture_group_id)});
        return texture_groups_.at(texture_group_id).get();
    }

    UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API texture_group_get_id(TextureGroup* texture_group)
    {
        return texture_group->id;
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_y_texture_view(TextureGroup* texture_group)
    {
        return texture_group->y_texture_view;
    }

    UNITY_INTERFACE_EXPORT ID3D11ShaderResourceView* UNITY_INTERFACE_API texture_group_get_uv_texture_view(TextureGroup* texture_group)
    {
        return texture_group->uv_texture_view;
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

    UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API texture_group_set_av_frame(TextureGroup* texture_group, tt::AVFrameHandle* av_frame)
    {
        texture_group->av_frame = std::move(*av_frame);
    }

    void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API texture_group_set_depth_pixels(TextureGroup* texture_group, std::vector<short>* depth_pixels)
    {
        texture_group->depth_pixels = std::move(*depth_pixels);
    }
}