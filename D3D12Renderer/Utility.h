#pragma once

#include <string>

namespace Utility
{
    std::wstring GetFileExtension(const std::wstring& filePath);
    std::wstring RemoveFileExtension(const std::wstring& filePath);
}