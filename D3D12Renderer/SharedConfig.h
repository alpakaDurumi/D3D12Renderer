// Config file that contains several variables used in both CPP and HLSL.

#ifndef SHARED_CONFIG
#define SHARED_CONFIG

#ifdef __cplusplus
// For CPP
#include <Windows.h>
constexpr UINT MAX_CASCADES = 4;
constexpr UINT POINT_LIGHT_ARRAY_SIZE = 6;
constexpr UINT SPOT_LIGHT_ARRAY_SIZE = 1;

enum class LightType
{
    DIRECTIONAL,
    POINT,
    SPOT,
    NUM_LIGHT_TYPES
};
#else   // __cplusplus
// For HLSL
static const uint MAX_CASCADES = 4;
static const uint POINT_LIGHT_ARRAY_SIZE = 6;
static const uint SPOT_LIGHT_ARRAY_SIZE = 1;

static const uint LIGHT_TYPE_DIRECTIONAL = 0;
static const uint LIGHT_TYPE_POINT = 1;
static const uint LIGHT_TYPE_SPOT = 2;
#endif  // __cplusplus

#endif  // SHARED_CONFIG