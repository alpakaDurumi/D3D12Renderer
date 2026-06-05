#include "pch.h"

#include "Utility.h"

#include <stdlib.h>

namespace Utility
{
std::wstring GetFileExtension(const std::wstring& filePath)
{
    size_t lastSlash = filePath.find_last_of(L"/\\");
    size_t lastDot = filePath.rfind(L'.');

    if (lastDot != std::wstring::npos && (lastSlash == std::wstring::npos || lastSlash < lastDot))
    {
        return filePath.substr(lastDot + 1);
    }
    else
    {
        return L"";
    }
}

std::wstring RemoveFileExtension(const std::wstring& filePath)
{
    return filePath.substr(0, filePath.rfind(L"."));
}

UINT CeilPowerOfTwo(UINT x)
{
    if (x <= 1)
        return 1;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;

    return x + 1;
}

std::size_t Align(std::size_t value, std::size_t alignment)
{
    return (value + (alignment - 1)) & ~(alignment - 1);
}
} // namespace Utility
