#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

// PIX Event
#ifdef USE_PIX
#include "pix3.h"
#define PIX_SCOPED_EVENT(cmdList, color, name) PIXScopedEvent(cmdList, color, name)
#else
#define PIX_SCOPED_EVENT(cmdList, color, name)
#endif // USE_PIX
