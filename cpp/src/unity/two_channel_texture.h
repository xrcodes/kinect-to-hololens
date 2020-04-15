#pragma once

#include <memory>
#include <d3d11.h>
#include <gsl/gsl>

namespace kh
{
class TwoChannelTexture
{
public:
	TwoChannelTexture(ID3D11Device* device, int width, int height);
	~TwoChannelTexture();
	int width() { return width_; }
	int height() { return height_; }
	ID3D11ShaderResourceView* getTextureView(ID3D11Device* device);
	void updatePixels(ID3D11DeviceContext* device_context,
					  uint8_t* frame_data1,
					  int frame_linesize1,
					  uint8_t* frame_data2,
					  int frame_linesize2);

private:
	int width_;
	int height_;
	ID3D11Texture2D* texture_;
};
}