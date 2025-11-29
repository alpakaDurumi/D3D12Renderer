#pragma once

#include <string>

namespace Utility
{
    std::wstring GetFileExtension(const std::wstring& filePath);
    std::wstring RemoveFileExtension(const std::wstring& filePath);

    unsigned long Djb2Hash(const std::wstring str);
    std::wstring MultiByteToWideChar(const std::string& str);

    // Boost hash_combine
    template <class T>
    inline void HashCombine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
}