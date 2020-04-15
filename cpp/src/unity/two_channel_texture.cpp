#include "two_channel_texture.h"

#include <string>

namespace kh
{
TwoChannelTexture::TwoChannelTexture(ID3D11Device* device, int width, int height)
	: width_(width), height_(height), texture_(nullptr)
{
    // D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE are chosen to update pixels in ChannelTexture::updatePixels().
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    HRESULT hr = device->CreateTexture2D(&desc, 0, &texture_);

    if (FAILED(hr)) {
        std::string str = "ChannelTexture::create failed, result: " + std::to_string(hr) + ", texture: " + std::to_string((uint64_t)texture_);
        throw std::exception(str.c_str());
    }
}

TwoChannelTexture::~TwoChannelTexture()
{
	if (texture_)
		texture_->Release();
}

ID3D11ShaderResourceView* TwoChannelTexture::getTextureView(ID3D11Device* device)
{
	ID3D11ShaderResourceView* texture_view;
	device->CreateShaderResourceView(texture_, 0, &texture_view);
	return texture_view;
}

// Update the pixels of the texture with a FFmpegFrame.
// index is to tell which channel (Y, U, or V) is this texture for.
void TwoChannelTexture::updatePixels(ID3D11DeviceContext* device_context,
								     uint8_t* frame_data1,
								     int frame_linesize1,
									 uint8_t* frame_data2,
									 int frame_linesize2)
{
	D3D11_MAPPED_SUBRESOURCE mapped;
	device_context->Map(texture_, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	int row_pitch = mapped.RowPitch;
	uint8_t* texture_data = reinterpret_cast<uint8_t*>(mapped.pData);

	for (int j = 0; j < height_; ++j) {
		int texture_offset = j * row_pitch;
		int frame_offset1 = j * frame_linesize1;
		int frame_offset2 = j * frame_linesize2;
		//memcpy(texture_data + texture_offset, frame_data1 + frame_offset1, width_);
		for (int i = 0; i < width_; ++i) {
			texture_data[texture_offset + i * 2] = frame_data1[frame_offset1 + i];
			texture_data[texture_offset + i * 2 + 1] = frame_data2[frame_offset2 + i];
;		}
	}

	device_context->Unmap(texture_, 0);
}
}