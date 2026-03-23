#pragma once

#include <functional>   // for std::hash
#include <string>

#include "Utility.h"

enum class PassType
{
    DEFAULT,
    DEPTH_ONLY,
    GBUFFER,
    DEFERRED_LIGHTING,
    NUM_PASS_TYPES
};

// Key that identify unique ShaderBlob.
struct ShaderKey
{
    std::wstring fileName;

    bool operator==(const ShaderKey& other) const;
};

// Specialization for hashing ShaderKey (required for std::unordered_map)
template<>
struct std::hash<ShaderKey>
{
    std::size_t operator()(const ShaderKey& key) const
    {
        std::hash<std::wstring> hasher;
        return hasher(key.fileName);
        }
};

// Key that identify unique PSO.
struct PSOKey
{
    PassType passType;  // 1

    std::wstring vsName;
    std::wstring psName;

    bool operator==(const PSOKey& other) const;
};

template<>
struct std::hash<PSOKey>
{
    std::size_t operator()(const PSOKey& key) const
    {
        size_t seed = 0;

        Utility::HashCombine(seed, static_cast<size_t>(key.passType));
        Utility::HashCombine(seed, key.vsName);
        Utility::HashCombine(seed, key.psName);

        return seed;
    }
};