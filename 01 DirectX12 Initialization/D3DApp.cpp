#include "D3DApp.h"

float D3DApp::AspectRatio() const
{
    return static_cast<float>(mClientWidth) / mClientHeight;
}
D3DApp::~D3DApp()
{
    if (md3dDevice != nullptr)
        FlushCommandQueue();
}
