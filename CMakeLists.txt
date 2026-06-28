cmake_minimum_required(VERSION 3.26)

project(
    D3D12Renderer
    LANGUAGES CXX
)

if(CMAKE_CONFIGURATION_TYPES AND NOT "Release_PIX" IN_LIST CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES Release_PIX)
endif()

set(CMAKE_CXX_FLAGS_RELEASE_PIX "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE_PIX "${CMAKE_EXE_LINKER_FLAGS_RELEASE}" CACHE STRING "" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE_PIX Release)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND WIN32)
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _llvm_bin)
    set(OpenMP_libomp_LIBRARY "${_llvm_bin}/../lib/libomp.lib")
    if(NOT EXISTS "${OpenMP_libomp_LIBRARY}")
        message(FATAL_ERROR
            "libomp.lib not found at ${OpenMP_libomp_LIBRARY}. "
            "This assumes the standard LLVM install layout, where the compiler is in "
            "<LLVM>/bin and libomp.lib is in the sibling <LLVM>/lib. "
            "If your environment differs, set -DOpenMP_libomp_LIBRARY=<path-to-libomp.lib> manually.")
    endif()
endif()
find_package(OpenMP REQUIRED)

include(FetchContent)

FetchContent_Declare(
    DirectXMath
    URL https://github.com/microsoft/DirectXMath/archive/refs/tags/oct2024.zip
    URL_HASH SHA256=b215087ddd085381839c6fc6f51185579f3a1c55804190b2ccc5969e9520b0bd
)
FetchContent_MakeAvailable(DirectXMath)

FetchContent_Declare(
    DirectXTex
    URL https://github.com/microsoft/DirectXTex/archive/refs/tags/oct2025.zip
    URL_HASH SHA256=e2204b0532c2df2cfa80f6550726d07905bc5107c40b31291c7b84a3522192cb
)
set(BUILD_TOOLS OFF)
set(BUILD_SAMPLE OFF)
set(BUILD_DX11 OFF)
set(BC_USE_OPENMP ON)
FetchContent_MakeAvailable(DirectXTex)

FetchContent_Declare(
    DirectX12Agility
    URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.717.1-preview
    URL_HASH SHA256=f052df9437c7e47cdb428ffb597481fe55a480c8968b0056159fe3a195c277e4
)
FetchContent_MakeAvailable(DirectX12Agility)

FetchContent_Declare(
    DXC
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2505.1/dxc_2025_07_14.zip
    URL_HASH SHA256=9ad895a6b039e3a8f8c22a1009f866800b840a74b50db9218d13319e215ea8a4
)
FetchContent_MakeAvailable(DXC)

find_program(DXC_EXE NAMES dxc PATHS ${dxc_SOURCE_DIR}/bin/x64 NO_DEFAULT_PATH REQUIRED)

FetchContent_Declare(
    WinPixEventRuntime
    URL https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.240308001
    URL_HASH SHA256=726acc93d6968e2146261a1e415521747d50ad69894c2b42b5d0d4c29fd66ec4
)
FetchContent_MakeAvailable(WinPixEventRuntime)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/bin)

add_executable(
    D3D12Renderer
    WIN32
    D3D12Renderer/Buffer.cpp
    D3D12Renderer/CacheKeys.cpp
    D3D12Renderer/Camera.cpp
    D3D12Renderer/CommandQueue.cpp
    D3D12Renderer/ConstantData.cpp
    D3D12Renderer/D3DHelper.cpp
    D3D12Renderer/DescriptorAllocation.cpp
    D3D12Renderer/DescriptorAllocator.cpp
    D3D12Renderer/DescriptorAllocatorPage.cpp
    D3D12Renderer/DynamicDescriptorHeap.cpp
    D3D12Renderer/FrameResource.cpp
    D3D12Renderer/GpuResource.cpp
    D3D12Renderer/ImGuiDescriptorAllocation.cpp
    D3D12Renderer/ImGuiDescriptorAllocator.cpp
    D3D12Renderer/Light.cpp
    D3D12Renderer/main.cpp
    D3D12Renderer/Material.cpp
    D3D12Renderer/Mesh.cpp
    D3D12Renderer/pch.cpp
    D3D12Renderer/Renderer.cpp
    D3D12Renderer/RootSignature.cpp
    D3D12Renderer/SceneHandles.cpp
    D3D12Renderer/Texture.cpp
    D3D12Renderer/TransientUploadAllocator.cpp
    D3D12Renderer/Utility.cpp
    D3D12Renderer/View.cpp
    D3D12Renderer/Win32Application.cpp

    D3D12Renderer/imgui/imgui_demo.cpp
    D3D12Renderer/imgui/imgui_draw.cpp
    D3D12Renderer/imgui/imgui_tables.cpp
    D3D12Renderer/imgui/imgui_widgets.cpp
    D3D12Renderer/imgui/imgui.cpp
    D3D12Renderer/imgui/backends/imgui_impl_dx12.cpp
    D3D12Renderer/imgui/backends/imgui_impl_win32.cpp

    D3D12Renderer/DDSTextureLoader/DDSTextureLoader12.cpp
)

set_source_files_properties(
    D3D12Renderer/imgui/imgui_demo.cpp
    D3D12Renderer/imgui/imgui_draw.cpp
    D3D12Renderer/imgui/imgui_tables.cpp
    D3D12Renderer/imgui/imgui_widgets.cpp
    D3D12Renderer/imgui/imgui.cpp
    D3D12Renderer/imgui/backends/imgui_impl_dx12.cpp
    D3D12Renderer/imgui/backends/imgui_impl_win32.cpp
    D3D12Renderer/DDSTextureLoader/DDSTextureLoader12.cpp
    PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
)

target_precompile_headers(
    D3D12Renderer
    PRIVATE
    D3D12Renderer/pch.h
)

target_compile_features(
    D3D12Renderer
    PRIVATE
    cxx_std_17
)

target_include_directories(
    D3D12Renderer
    BEFORE
    PRIVATE
    ${directx12agility_SOURCE_DIR}/build/native/include
    ${directx12agility_SOURCE_DIR}/build/native/include/d3dx12
)

target_include_directories(
    D3D12Renderer
    PRIVATE
    D3D12Renderer/imgui
    D3D12Renderer/imgui/backends
    D3D12Renderer/DDSTextureLoader
    ${winpixeventruntime_SOURCE_DIR}/Include/WinPixEventRuntime
)

add_custom_command(
    TARGET D3D12Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:D3D12Renderer>/D3D12
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${directx12agility_SOURCE_DIR}/build/native/bin/x64/D3D12Core.dll
        ${directx12agility_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.dll
        $<TARGET_FILE_DIR:D3D12Renderer>/D3D12
)

add_custom_command(
    TARGET D3D12Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Release_PIX>,copy_if_different,true>
        ${winpixeventruntime_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll
        $<TARGET_FILE_DIR:D3D12Renderer>
)

target_compile_definitions(
    D3D12Renderer
    PRIVATE
    UNICODE
    _UNICODE
    $<$<CONFIG:Release_PIX>:USE_PIX>
)

target_compile_options(
    D3D12Renderer
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
)

target_link_libraries(
    D3D12Renderer
    PRIVATE
    d3d12
    dxgi
    winmm
    $<$<CONFIG:Debug>:dxguid>
    OpenMP::OpenMP_CXX
    
    DirectXMath
    DirectXTex
    $<$<CONFIG:Release_PIX>:${winpixeventruntime_SOURCE_DIR}/bin/x64/WinPixEventRuntime.lib>
)

set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/D3D12Renderer)
set(SHADER_CSO_OUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/shaders)
set(SHADER_PDB_OUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/intermediate/pdb)
set(SHADER_DEP_OUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/intermediate/dep)

set(DXC_COMMON -nologo -HV 2016 -I ${SHADER_SRC})
set(DXC_CONFIG $<$<NOT:$<CONFIG:Release>>:-Od> $<$<NOT:$<CONFIG:Release>>:-Zi> $<$<CONFIG:Release>:-O3>)

function(compile_shader)
    cmake_parse_arguments(ARG "" "SOURCE;OUTPUT;PROFILE" "DEFINES" ${ARGN})
    
    set(define_flags "")
    foreach(d IN LISTS ARG_DEFINES)
        list(APPEND define_flags -D ${d})
    endforeach()

    add_custom_command(
        OUTPUT ${SHADER_CSO_OUT}/${ARG_OUTPUT}.cso
        BYPRODUCTS $<$<NOT:$<CONFIG:Release>>:${SHADER_PDB_OUT}/${ARG_OUTPUT}.pdb>
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_CSO_OUT} ${SHADER_PDB_OUT} ${SHADER_DEP_OUT}
        COMMAND ${DXC_EXE} ${SHADER_SRC}/${ARG_SOURCE}.hlsl
            -E main -T ${ARG_PROFILE}
            ${define_flags}
            ${DXC_COMMON}
            ${DXC_CONFIG}
            $<$<NOT:$<CONFIG:Release>>:-Fd>
            $<$<NOT:$<CONFIG:Release>>:${SHADER_PDB_OUT}/${ARG_OUTPUT}.pdb>
            -Fo ${SHADER_CSO_OUT}/${ARG_OUTPUT}.cso
        COMMAND ${DXC_EXE} ${SHADER_SRC}/${ARG_SOURCE}.hlsl
            -E main -T ${ARG_PROFILE}
            ${define_flags}
            ${DXC_COMMON}
            -MF ${SHADER_DEP_OUT}/${ARG_OUTPUT}.d
        DEPENDS ${SHADER_SRC}/${ARG_SOURCE}.hlsl
        DEPFILE ${SHADER_DEP_OUT}/${ARG_OUTPUT}.d
        COMMENT "DXC: ${ARG_SOURCE}.hlsl -> ${ARG_OUTPUT}.cso"
        VERBATIM
    )

    set_property(GLOBAL APPEND PROPERTY SHADER_OUTPUTS ${SHADER_CSO_OUT}/${ARG_OUTPUT}.cso)
endfunction()

compile_shader(SOURCE MeshVS               OUTPUT MeshVS               PROFILE vs_6_0)
compile_shader(SOURCE MeshVS               OUTPUT MeshVS_depth_only    PROFILE vs_6_0 DEFINES DEPTH_ONLY=1)
compile_shader(SOURCE FullScreenTriangleVS OUTPUT FullScreenTriangleVS PROFILE vs_6_0)
compile_shader(SOURCE ForwardColoringPS    OUTPUT ForwardColoringPS    PROFILE ps_6_0)
compile_shader(SOURCE PointLightShadowPS   OUTPUT PointLightShadowPS   PROFILE ps_6_0 DEFINES DEPTH_ONLY=1)
compile_shader(SOURCE GBufferPS            OUTPUT GBufferPS            PROFILE ps_6_0)
compile_shader(SOURCE DeferredLightingPS   OUTPUT DeferredLightingPS   PROFILE ps_6_0)
compile_shader(SOURCE OutlinePS            OUTPUT OutlinePS            PROFILE ps_6_0)
compile_shader(SOURCE ToneMapPS            OUTPUT ToneMapPS            PROFILE ps_6_0)
compile_shader(SOURCE SelectionMaskPS      OUTPUT SelectionMaskPS      PROFILE ps_6_0 DEFINES DEPTH_ONLY=1)
compile_shader(SOURCE HorizontalDilatePS   OUTPUT HorizontalDilatePS   PROFILE ps_6_0)

get_property(shader_outputs GLOBAL PROPERTY SHADER_OUTPUTS)

add_custom_target(
    Shaders
    ALL
    DEPENDS
    ${shader_outputs}
)

add_custom_command(
    TARGET D3D12Renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:D3D12Renderer>/assets
    COMMENT "Copying assets"
)
