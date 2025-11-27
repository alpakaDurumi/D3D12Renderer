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

    unsigned long Djb2Hash(const std::wstring str)
    {
        unsigned long hash = 5381;

        for (wchar_t c : str)
            hash = ((hash << 5) + hash) + c;    // hash * 33 + c

        return hash;
    }
    
    std::wstring MultiByteToWideChar(const std::string& str)
    {
        std::wstring converted;

        // Calculate required length and resize dest buffer
        size_t requiredSize;
        mbstowcs_s(&requiredSize, nullptr, 0, str.data(), 0);
        converted.resize(requiredSize);
        
        // Convert
        mbstowcs_s(nullptr, converted.data(), requiredSize, str.data(), requiredSize - 1);

        // Truncate null character
        converted.resize(requiredSize - 1);

        return converted;
    }
}