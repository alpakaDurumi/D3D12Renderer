#include "pch.h"
#include "CacheKeys.h"

// Equality operator (required for std::unordered_map)
bool ShaderKey::operator==(const ShaderKey& other) const
{
    return fileName == other.fileName;
}

bool PSOKey::operator==(const PSOKey& other) const
{
    return passType == other.passType &&
        vsName == other.vsName &&
        psName == other.psName;
}