#include "pch.h"
#include "CacheKeys.h"

// Equality operator (required for std::unordered_map)
bool RSKey::operator==(const RSKey& other) const
{
    return filtering == other.filtering &&
        addressingMode == other.addressingMode;
}

bool ShaderKey::operator==(const ShaderKey& other) const
{
    return fileName == other.fileName &&
        defines == other.defines &&
        target == other.target;
}

bool ShaderKey::IsEmpty() const
{
    return fileName.empty() && defines.empty() && target.empty();
}

bool PSOKey::operator==(const PSOKey& other) const
{
    return filtering == other.filtering &&
        addressingMode == other.addressingMode &&
        meshType == other.meshType &&
        passType == other.passType &&
        vsKey == other.vsKey &&
        psKey == other.psKey;
}