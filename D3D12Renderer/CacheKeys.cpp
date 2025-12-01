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
        target == other.target &&
        defines == other.defines;
}

bool PSOKey::operator==(const PSOKey& other) const
{
    return filtering == other.filtering &&
        addressingMode == other.addressingMode &&
        meshType == other.meshType &&
        vsKey == other.vsKey &&
        psKey == other.psKey;
}