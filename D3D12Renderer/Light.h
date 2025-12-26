#pragma once

#include <Windows.h>

#include "ConstantData.h"

class Light
{
    // TODO : Implement constructor and add fields (pos, dir, etc...) and getter functions

public:
    CameraConstantData m_cameraConstantData;
    UINT m_cameraConstantBufferIndex;

    LightConstantData m_lightConstantData;
    UINT m_lightConstantBufferIndex;
};