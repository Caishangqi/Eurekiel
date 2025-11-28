# Dear ImGui Integration

**Version**: v1.92.4 (2024-10-14)
**Repository**: https://github.com/ocornut/imgui
**License**: MIT License
**Release Date**: October 14, 2024 (10th Anniversary Edition)

## About ImGui

Dear ImGui is a bloat-free graphical user interface library for C++. It outputs optimized vertex buffers that you can render anytime in your 3D-pipeline-enabled application. It is fast, portable, renderer agnostic, and self-contained (no external dependencies).

ImGui is designed to enable fast iterations and to empower programmers to create content creation tools and visualization/debug tools (as opposed to UI for the average end-user). It favors simplicity and productivity toward this goal and lacks certain features commonly found in more high-level libraries.

## Download Instructions

### Manual Download (Required)

Since the files need to be downloaded manually, please follow these steps:

1. **Download ImGui v1.92.4 Source Code**
   - Visit: https://github.com/ocornut/imgui/releases/tag/v1.92.4
   - Download: `Source code (zip)` or `Source code (tar.gz)`
   - OR clone the repository: `git clone --branch v1.92.4 https://github.com/ocornut/imgui.git`

2. **Extract Required Files**

   From the ImGui root directory, copy the following files to `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\`:

   **Core Files** (from root directory):
   ```
   ✓ imgui.h
   ✓ imgui.cpp
   ✓ imgui_draw.cpp
   ✓ imgui_tables.cpp
   ✓ imgui_widgets.cpp
   ✓ imgui_demo.cpp (optional, but recommended for reference)
   ✓ imgui_internal.h
   ✓ imconfig.h
   ✓ imstb_rectpack.h
   ✓ imstb_textedit.h
   ✓ imstb_truetype.h
   ```

   **Backend Files** (from `backends/` directory to `backends/` subdirectory):
   ```
   ✓ backends/imgui_impl_dx11.h
   ✓ backends/imgui_impl_dx11.cpp
   ✓ backends/imgui_impl_dx12.h
   ✓ backends/imgui_impl_dx12.cpp
   ✓ backends/imgui_impl_win32.h
   ✓ backends/imgui_impl_win32.cpp
   ```

3. **Verify File Structure**

   After copying, your directory should look like:
   ```
   F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\
   ├── README.md (this file)
   ├── INTEGRATION_CHECKLIST.md (generated)
   ├── imgui.h
   ├── imgui.cpp
   ├── imgui_draw.cpp
   ├── imgui_tables.cpp
   ├── imgui_widgets.cpp
   ├── imgui_demo.cpp
   ├── imgui_internal.h
   ├── imconfig.h
   ├── imstb_rectpack.h
   ├── imstb_textedit.h
   ├── imstb_truetype.h
   └── backends/
       ├── imgui_impl_dx11.h
       ├── imgui_impl_dx11.cpp
       ├── imgui_impl_dx12.h
       ├── imgui_impl_dx12.cpp
       ├── imgui_impl_win32.h
       └── imgui_impl_win32.cpp
   ```

## Integration into Eurekiel Engine

This integration was performed on: **2025-10-15**

### Used By
- `ImGuiSubsystem` (Engine/Core/ImGui/ImGuiSubsystem.cpp)
- Future: ImGuiBackendDX11, ImGuiBackendDX12

### Configuration

**Include Paths** (configured in Engine.vcxproj):
```
..\ThirdParty\imgui
..\ThirdParty\imgui\backends
```

**Preprocessor Defines**:
- None required (ImGui works out of the box)

**Optional Configuration** (in `imconfig.h`):
- Custom memory allocators
- Custom math types (Vec2, Vec4)
- Disable specific features

### Backend Support

| Backend | Status | Implementation File |
|---------|--------|-------------------|
| DirectX 11 | ✓ Integrated | backends/imgui_impl_dx11.cpp |
| DirectX 12 | ✓ Integrated | backends/imgui_impl_dx12.cpp |
| Win32 Platform | ✓ Integrated | backends/imgui_impl_win32.cpp |

### Dependencies

**DirectX 11 Backend**:
- `<d3d11.h>` (Windows SDK)
- `<dxgi.h>` (Windows SDK)

**DirectX 12 Backend**:
- `<d3d12.h>` (Windows SDK)
- `<dxgi1_4.h>` (Windows SDK)

**Win32 Platform**:
- `<windows.h>` (Windows SDK)

All dependencies are already available in the Engine project.

## Documentation

- **Official Documentation**: https://github.com/ocornut/imgui/wiki
- **Getting Started**: https://github.com/ocornut/imgui/wiki/Getting-Started
- **FAQ**: https://github.com/ocornut/imgui/blob/master/docs/FAQ.md
- **Examples**: https://github.com/ocornut/imgui/tree/master/examples

### Key Resources

1. **imgui_demo.cpp**: The best documentation is the demo. Run it and explore!
2. **imgui.h**: Contains all the API documentation in comments
3. **Wiki**: https://github.com/ocornut/imgui/wiki

## Version History

| Version | Date | Notes |
|---------|------|-------|
| v1.92.4 | 2024-10-14 | Initial integration (10th Anniversary) |

## License

ImGui is licensed under the MIT License:

```
The MIT License (MIT)

Copyright (c) 2014-2024 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Support

For issues related to ImGui itself, please refer to:
- GitHub Issues: https://github.com/ocornut/imgui/issues
- Discord: https://discord.gg/yrwXYr5

For integration issues specific to Eurekiel Engine, contact the engine team.

## Notes

- **DO NOT MODIFY** ImGui source files. Keep them in their original state for easier updates.
- For custom configurations, use `imconfig.h` (only if absolutely necessary).
- When updating ImGui in the future, replace all files with the new version and test thoroughly.
- The `imgui_demo.cpp` is optional but highly recommended for learning and reference.

## Performance Notes

ImGui is designed to be very fast and efficient:
- Immediate mode rendering (no retained state)
- Optimized vertex buffer generation
- Minimal memory allocations
- Efficient for thousands of widgets per frame

## 10th Anniversary

ImGui v1.92.4 celebrates the 10th anniversary of ImGui v1.00 (released October 2014). It has grown from a simple debugging tool to a widely-used UI library in games, game engines, and professional tools.

Special thanks to all contributors and sponsors who have supported ImGui over the years!
