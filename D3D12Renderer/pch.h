#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Windows
#include <Windows.h>
#include <shellapi.h>
#include <wrl/client.h>

// D3D12
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>

// C++ Standard Library
#include <cstdint>
#include <cassert>
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <mutex>
#include <unordered_map>
#include <optional>
#include <utility>
#include <exception>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// PIX Event
#ifdef USE_PIX
#include "pix3.h"
#define PIX_SCOPED_EVENT(cmdList, color, name) PIXScopedEvent(cmdList, color, name)
#else
#define PIX_SCOPED_EVENT(cmdList, color, name)
#endif  // USE_PIX