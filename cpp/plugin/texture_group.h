#pragma once

#include <d3d11.h>

void texture_group_init(ID3D11Device* device);
void texture_group_update_rvl(ID3D11Device* device, ID3D11DeviceContext* device_context);