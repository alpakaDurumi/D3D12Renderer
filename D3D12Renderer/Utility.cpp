#include "pch.h"
#include "Utility.h"

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
}