# D3D12Renderer

Custom 3D rendering engine using Direct3D 12, C++17 and Win32 API

## Used

### Packages

| Package                  | Version         |
| ------------------------ | --------------- |
| directxmath              | 2024.10.15.1    |
| directxtex_desktop_win10 | 2025.10.28.1    |
| Microsoft.Direct3D.D3D12 | 1.717.1-preview |
| Microsoft.Direct3D.DXC   | 1.8.2505.32     |
| WinPixEventRuntime       | 1.0.240308001   |

### Included files or codes

- [ImGui](https://github.com/ocornut/imgui)
- [DDSTextureLoader12](https://github.com/microsoft/DirectXTex/tree/main/DDSTextureLoader) from DirectXTex
- [ThrowIfFailed](https://github.com/Microsoft/DirectXTK/wiki/ThrowIfFailed)
- [GetLatestWinPixGpuCapturerPath_Cpp17](https://devblogs.microsoft.com/pix/taking-a-capture/)

### Assets

- https://ambientcg.com/a/PavingStones150

## Build

### Prerequisites

- CMake 3.26+
- Windows SDK
- Visual Studio 2022 Build Tools with the "Desktop development with C++" workload
- Microsoft Visual C++ Redistributable (x64)
- LLVM, Ninja (If you use provided presets)
- This project uses the **preview release** of the D3D12 Agility SDK to use *Enhanced Barriers*, and it can only be loaded when **Windows Developer Mode** is enabled. To enable Developer Mode, open Settings and search for 'Developer Mode', then turn it on.
- Download assets from [here](https://drive.google.com/drive/folders/1CGZupYVKDUj7CQFzVJCZiTwuaJ--kPri?usp=sharing) and copy `assets` folder to repo root directory.

### Instructions

Quick start:

```bash
cmake --preset clang-cl
cmake --build --preset release
```

- You can choose `debug` build preset.
- You can also use any generator or compiler for your preference.

#### Options

All options default to `OFF` and are independent of the build configuration (Debug/Release). Set them at **configure** time:

```bash
cmake --preset clang-cl -DENGINE_DEBUG_LAYER=ON -DENGINE_PIX=ON
cmake --build --preset release
```

| Option                        | Effect                                                                                 |
| ----------------------------- | -------------------------------------------------------------------------------------- |
| `ENGINE_DEBUG_LAYER`          | Enables the D3D12/DXGI debug layer and info-queues                                     |
| `ENGINE_GPU_BASED_VALIDATION` | Enables GPU-based validation (GBV). It only works when `ENGINE_DEBUG_LAYER` is enabled |
| `ENGINE_SHADER_DEBUG`         | Compiles shaders without optimization and with debug info, emits PDBs                  |
| `ENGINE_PIX`                  | Enables WinPixEventRuntime markers for PIX captures                                    |

## References

- [Direct3D 12 graphics - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [DirectX-Specs](https://microsoft.github.io/DirectX-Specs/)
- [Microsoft's DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Learning DirectX 12 series by 3Dgep (Jeremiah van Oosten)](https://www.3dgep.com/category/graphics-programming/directx/)
- [DXC guide](https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll)
- [Game Engine Architecture by Jason Gregory](https://www.gameenginebook.com/)
- Detail about
    - The wiki page of DirectXTK repo has good explanations.
    - [ComPtr](https://github.com/Microsoft/DirectXTK/wiki/ComPtr)
