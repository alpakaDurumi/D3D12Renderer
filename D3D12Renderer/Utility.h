#pragma once

#include <string>

namespace Utility
{
    std::wstring GetFileExtension(const std::wstring& filePath);
    std::wstring RemoveFileExtension(const std::wstring& filePath);

    unsigned long Djb2Hash(const std::wstring str);
    std::wstring MultiByteToWideChar(const std::string& str);
}