#pragma once

#include <minwindef.h>

inline constexpr UINT FrameCount = 2;

enum class TextureFiltering
{
    POINT,
    BILINEAR,
    ANISOTROPIC_X2,
    ANISOTROPIC_X4,
    ANISOTROPIC_X8,
    ANISOTROPIC_X16,
    NUM_TEXTURE_FILTERINGS
};

enum class TextureAddressingMode
{
    WRAP,
    MIRROR,
    CLAMP,
    BORDER,
    MIRROR_ONCE,
    NUM_TEXTURE_ADDRESSING_MODES
};