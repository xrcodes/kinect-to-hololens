#pragma once

#include <memory>
#include <vector>
#define NOMINMAX
#include <d3d11.h>
#undef NOMINMAX

namespace tt
{
// A class for the texture for Kinect depth pixels.
class DepthTexture
{
public:
    DepthTexture(ID3D11Device* device, int width, int height);
    ~DepthTexture();
    int width() { return width_; }
    int height() { return height_; }
    ID3D11ShaderResourceView* getTextureView(ID3D11Device* device);
	void updatePixels(ID3D11Device* device,
					  ID3D11DeviceContext* device_context,
					  int width,
					  int height,
                      uint16_t* pixels);

private:
    int width_;
    int height_;
    ID3D11Texture2D* texture_;
};
}