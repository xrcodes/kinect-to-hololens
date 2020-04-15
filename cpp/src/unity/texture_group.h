#pragma once

#include <memory>
#include <d3d11.h>
#include "kh_trvl.h"
#include "channel_texture.h"
#include "two_channel_texture.h"
#include "depth_texture.h"

struct TextureGroup
{
public:
    const int id;

    // Color and depth texture sizes.
    int width;
    int height;

    // Instances of classes for Direct3D textures.
    std::unique_ptr<kh::ChannelTexture> y_texture{nullptr};
    std::unique_ptr<kh::TwoChannelTexture> uv_texture{nullptr};
    std::unique_ptr<kh::DepthTexture> depth_texture{nullptr};

    // Unity connects Unity textures to Direct3D textures through creating Unity textures binded to these texture views.
    ID3D11ShaderResourceView* y_texture_view{nullptr};
    ID3D11ShaderResourceView* uv_texture_view{nullptr};
    ID3D11ShaderResourceView* depth_texture_view{nullptr};

    // These variables get set in the main thread of Unity, then gets assigned to textures in the render thread of Unity.
    kh::FFmpegFrame ffmpeg_frame{nullptr};
    std::unique_ptr<kh::TrvlDecoder> depth_decoder;
    std::vector<short> depth_pixels;

    TextureGroup(int id) : id{id} {};
};

void texture_group_init(int texture_group_id, ID3D11Device* device);
void texture_group_update(int texture_group_id, ID3D11Device* device, ID3D11DeviceContext* device_context);