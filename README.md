# D3D12Renderer

Custom 3D rendering engine using Direct3D 12, C++17 and Win32 API

# Used

- **Visual Studio 2026** with platform toolset **v145**
- **Windows 10.0 SDK**

## Served as Nuget

|Package|Version|
|--|--|
|DirectXMath|2024.10.15.1|
|DirectXTex|2025.10.28.1|
|DirectX 12 Agility SDK|1.717.1-preview|

## Included files or codes

- [ImGui](https://github.com/ocornut/imgui)
- [DDSTextureLoader12](https://github.com/microsoft/DirectXTex/tree/main/DDSTextureLoader) from DirectXTex
- [ThrowIfFailed](https://github.com/Microsoft/DirectXTK/wiki/ThrowIfFailed)
- [GetLatestWinPixGpuCapturerPath_Cpp17](https://devblogs.microsoft.com/pix/taking-a-capture/)

## Assets

- https://ambientcg.com/a/PavingStones150

# Build

1. Download assets from [here](https://drive.google.com/drive/folders/1CGZupYVKDUj7CQFzVJCZiTwuaJ--kPri?usp=sharing)
2. copy `Assets` folder to `D3D12Renderer` project directory
3. Open `D3D12Renderer.sln` solution and build the project (Debug/Release)

# References

- [Direct3D 12 graphics - Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-graphics)
- [DirectX-Specs](https://microsoft.github.io/DirectX-Specs/)
- [Microsoft's DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples)
- [Learning DirectX 12 series by 3Dgep (Jeremiah van Oosten)](https://www.3dgep.com/category/graphics-programming/directx/)
- [DXC guide](https://github.com/microsoft/DirectXShaderCompiler/wiki/Using-dxc.exe-and-dxcompiler.dll)
- [Game Engine Architecture by Jason Gregory](https://www.gameenginebook.com/)
- Detail about
    - The wiki page of DirectXTK repo has good explanations.
    - [ComPtr](https://github.com/Microsoft/DirectXTK/wiki/ComPtr)
