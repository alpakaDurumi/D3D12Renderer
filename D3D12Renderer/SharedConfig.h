// Config file that contains several variables used in both CPP and HLSL.

#ifndef SHARED_CONFIG
#define SHARED_CONFIG

#ifdef __cplusplus
#include <Windows.h>
constexpr UINT MAX_CASCADES = 4;
#else   // For HLSL
static const uint MAX_CASCADES = 4;
#endif

#endif