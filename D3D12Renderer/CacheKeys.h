#pragma once

#include <functional>   // for std::hash
#include <vector>
#include <string>

#include "Utility.h"

enum class MeshType
{
    DEFAULT,
    INSTANCED,
    NUM_MESH_TYPES
};

enum class PassType
{
    DEFAULT,
    DEPTH_ONLY,
    NUM_PASS_TYPES
};

// Key that identify unique ShaderBlob.
// We Assume that defines already sorted.
struct ShaderKey
{
    std::wstring fileName;
    std::vector<std::string> defines;
    std::string target;

    bool operator==(const ShaderKey& other) const;

    bool IsEmpty() const;
};

// Specialization for hashing ShaderKey (required for std::unordered_map)
template<>
struct std::hash<ShaderKey>
{
    std::size_t operator()(const ShaderKey& key) const
    {
        std::wstring combinedString = L"";
        combinedString += key.fileName;
        for (const std::string& define : key.defines)
        {
            combinedString += L"|" + Utility::MultiByteToWideChar(define);
        }

        if (!key.target.empty())
        {
            combinedString += L"|" + Utility::MultiByteToWideChar(key.target);
        }

        return static_cast<size_t>(Utility::Djb2Hash(combinedString));
    }
};

// Key that identify unique PSO.
struct PSOKey
{
    MeshType meshType;  // 1
    PassType passType;  // 1

    ShaderKey vsKey;
    ShaderKey psKey;

    bool operator==(const PSOKey& other) const;
};

template<>
struct std::hash<PSOKey>
{
    std::size_t operator()(const PSOKey& key) const
    {
        size_t seed = 0;

        size_t combinedBits =
            (static_cast<size_t>(key.meshType) << 1) |
            static_cast<size_t>(key.passType);

        Utility::HashCombine(seed, combinedBits);
        Utility::HashCombine(seed, key.vsKey);
        Utility::HashCombine(seed, key.psKey);

        return seed;
    }
};