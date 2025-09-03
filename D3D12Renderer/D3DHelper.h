#pragma once

#include <Windows.h>
#include <exception>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

namespace D3DHelper
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::exception();
        }
    }

    // Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
    // If no such adapter can be found, *ppAdapter will be set to nullptr.
    inline void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = true)
    {
        *ppAdapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter;

        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    IID_PPV_ARGS(&adapter)));
                    ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (adapter.Get() == nullptr)
        {
            for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *ppAdapter = adapter.Detach();
    }

    inline void MoveCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = SIZE_T(INT64(handle->ptr) + INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
    }

    inline void MoveGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = handle->ptr + UINT64(offsetInDescriptors) * UINT64(descriptorIncrementSize);
    }

    inline void MoveCPUAndGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
        INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        MoveCPUDescriptorHandle(cpuHandle, offsetInDescriptors, descriptorIncrementSize);
        MoveGPUDescriptorHandle(gpuHandle, offsetInDescriptors, descriptorIncrementSize);
    }

    inline void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges)
    {
        D3D12_DESCRIPTOR_RANGE tempRange = {};
        for (UINT i = 0; i < NumDescriptorRanges; i++)
        {
            tempRange.RangeType = src[i].RangeType;
            tempRange.NumDescriptors = src[i].NumDescriptors;
            tempRange.BaseShaderRegister = src[i].BaseShaderRegister;
            tempRange.RegisterSpace = src[i].RegisterSpace;
            tempRange.OffsetInDescriptorsFromTableStart = src[i].OffsetInDescriptorsFromTableStart;
            convertedRanges.push_back(tempRange);
        }
    }

    inline void DowngradeRootDescriptor(D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst)
    {
        dst->ShaderRegister = src->ShaderRegister;
        dst->RegisterSpace = src->RegisterSpace;
    }

    inline void DowngradeRootParameters(D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset)
    {
        for (UINT i = 0; i < numParameters; i++)
        {
            dst[i].ParameterType = src[i].ParameterType;

            const D3D12_ROOT_PARAMETER_TYPE& type = src[i].ParameterType;
            switch (type)
            {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
                const UINT NumDescriptorRanges = src[i].DescriptorTable.NumDescriptorRanges;
                DowngradeDescriptorRanges(src[i].DescriptorTable.pDescriptorRanges, NumDescriptorRanges, convertedRanges);
                dst[i].DescriptorTable = { NumDescriptorRanges, convertedRanges.data() + offset };
                offset += NumDescriptorRanges;
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
                dst[i].Constants = src[i].Constants;
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
                DowngradeRootDescriptor(&src[i].Descriptor, &dst[i].Descriptor);
                break;
            }
            }

            dst[i].ShaderVisibility = src[i].ShaderVisibility;
        }
    }
}