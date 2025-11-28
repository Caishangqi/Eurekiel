# ImGui集成快速入门

本文档提供ImGui集成到Eurekiel Engine的快速入门指南。

---

## 🚀 三步完成集成

### 第1步: 下载ImGui源码

**使用���动化脚本**(推荐):

```powershell
cd "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui"
.\download_imgui.ps1 -CleanTemp
```

**或手动下载**:

1. 访问: https://github.com/ocornut/imgui/archive/refs/tags/v1.92.4.zip
2. 下载并解压
3. 参考 `DOWNLOAD_GUIDE.md` 中的详细步骤

---

### 第2步: 验证集成

运行验证脚本检查所有文件是否正确:

```powershell
.\verify_imgui.ps1
```

**期望输出**:

```
✓ ImGui集成验证通过!

总检查项: 50
通过: 50
失败: 0
通过率: 100%
```

如果验证失败,使用详细模式查看问题:

```powershell
.\verify_imgui.ps1 -Detailed
```

---

### 第3步: 编译Engine项目

1. **打开Visual Studio 2022**

2. **加载解决方案**
   ```
   F:\p4\Personal\SD\Engine\Engine.sln
   ```

3. **重新加载Engine项目**(如果已打开)
   - 右键点击 `Engine` 项目
   - 选择 "Reload Project"

4. **编译Engine项目**
   ```
   Build → Build Engine
   ```
   或使用MSBuild:
   ```cmd
   msbuild F:\p4\Personal\SD\Engine\Engine.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
   ```

5. **检查编译输出**
   - 无ImGui相关错误
   - 无链接错误
   - Engine.lib成功生成

---

## ✅ 验证编译成功

### 编译日志中应该看到:

```
1>------ Build started: Project: Engine, Configuration: Debug x64 ------
1>imgui.cpp
1>imgui_draw.cpp
1>imgui_tables.cpp
1>imgui_widgets.cpp
1>imgui_demo.cpp
1>imgui_impl_dx11.cpp
1>imgui_impl_dx12.cpp
1>imgui_impl_win32.cpp
...
1>Engine.vcxproj -> F:\p4\Personal\SD\Engine\Temporary\Engine_x64_Debug\Engine.lib
========== Build: 1 succeeded, 0 failed, 0 up-to-date, 0 skipped ==========
```

### 如果编译失败:

**错误类型A: 找不到imgui.h**

```
fatal error C1083: Cannot open include file: 'imgui.h': No such file or directory
```

**解决方案**:
1. 运行 `.\verify_imgui.ps1` 检查文件是否存在
2. 确认 `Engine.vcxproj` 中的包含路径已更新
3. 重新加载Visual Studio解决方案

**错误类型B: 链接错误(LNK2019)**

```
unresolved external symbol "void __cdecl ImGui::NewFrame(void)"
```

**解决方案**:
1. 确认所有ImGui cpp文件都在 `Engine.vcxproj` 的 `<ClCompile>` 组中
2. 清理并重新编译: `Build → Clean Engine`, 然后 `Build → Build Engine`

---

## 🎯 下一步: 测试ImGui功能

### 在EurekielFeatureTest中添加测试代码

编辑 `F:\p4\Personal\SD\EurekielFeatureTest\Code\Game\App.cpp`:

```cpp
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"

void App::Startup()
{
    // ... 现有代码 ...

    // 获取ImGuiSubsystem
    auto* imguiSubsystem = g_theSubsystems->GetSubsystem<enigma::core::ImGuiSubsystem>();

    if (imguiSubsystem)
    {
        // 注册测试窗口
        imguiSubsystem->RegisterWindow("Debug Info", []() {
            ImGui::Begin("Debug Info");
            ImGui::Text("Application: EurekielFeatureTest");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Separator();
            if (ImGui::Button("Test Button"))
            {
                DebuggerPrintf("ImGui test button clicked!\n");
            }
            ImGui::End();
        });

        // 注册ImGui Demo窗口
        imguiSubsystem->RegisterWindow("ImGui Demo", []() {
            ImGui::ShowDemoWindow();
        });

        DebuggerPrintf("[App] ImGui test windows registered\n");
    }
}
```

### 编译并运行

1. 编译 `EurekielFeatureTest` 项目
2. 运行 `Run/EurekielFeatureTest_Debug_x64.exe`
3. 应该看到ImGui窗口显示

---

## 📚 完整文档索引

在 `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\` 目录下:

| 文档 | 用途 |
|------|------|
| **QUICKSTART.md** | 快速入门(本文档) |
| **DOWNLOAD_GUIDE.md** | 详细的下载和安装步骤 |
| **INTEGRATION_CHECKLIST.md** | 完整的集成验证清单 |
| **README.md** | ImGui库信息和文档链接 |
| **verify_imgui.ps1** | 自动验证脚本 |

---

## 🛠️ 常见问题

### Q1: 如何确认ImGui已正确集成?

运行验证脚本:
```powershell
.\verify_imgui.ps1
```

### Q2: 如何更新ImGui到新版本?

1. 下载新版本源码
2. 替换 `imgui/` 目录中的所有文件
3. 运行 `.\verify_imgui.ps1` 验证
4. 重新编译Engine项目

### Q3: 如何禁用ImGui Demo窗口?

在 `Engine.vcxproj` 中注释掉:
```xml
<!-- <ClCompile Include="..\ThirdParty\imgui\imgui_demo.cpp"/> -->
```

### Q4: 如何自定义ImGui样式?

在 `ImGuiSubsystem::InitializeImGuiContext()` 中:

```cpp
// 使用不同的内置样式
ImGui::StyleColorsDark();    // 深色主题(默认)
ImGui::StyleColorsLight();   // 浅色主题
ImGui::StyleColorsClassic(); // 经典主题

// 或自定义样式
ImGuiStyle& style = ImGui::GetStyle();
style.WindowRounding = 5.0f;
style.FrameRounding = 3.0f;
// ... 更多自定义 ...
```

### Q5: 如何添加自定义字体?

在 `ImGuiSubsystemConfig` 中设置:

```cpp
ImGuiSubsystemConfig config;
config.defaultFontPath = "path/to/font.ttf";
config.defaultFontSize = 16.0f;
```

---

## 🎓 学习资源

### ImGui官方文档

- **官方Wiki**: https://github.com/ocornut/imgui/wiki
- **Getting Started**: https://github.com/ocornut/imgui/wiki/Getting-Started
- **FAQ**: https://github.com/ocornut/imgui/blob/master/docs/FAQ.md

### 代码示例

- **imgui_demo.cpp**: 最好的学习资源,包含所有控件的示例
- **官方Examples**: https://github.com/ocornut/imgui/tree/master/examples

### 社区资源

- **Discord**: https://discord.gg/yrwXYr5
- **GitHub Issues**: https://github.com/ocornut/imgui/issues
- **Reddit**: r/cpp, r/gamedev

---

## 🚨 注意事项

### 重要提示

1. **不要修改ImGui源码** - 保持原始状态,便于更新
2. **使用ImGuiSubsystem** - 通过Engine的封装使用ImGui,不要直接调用
3. **检查初始化顺序** - 确保ImGuiSubsystem在渲染系统之后初始化
4. **注意线程安全** - ImGui不是线程安全的,只在主线程使用

### 性能考虑

- ImGui非常高效,但仍需注意:
  - 避免每帧创建大量窗口
  - 避免在循环中进行复杂计算
  - 使用 `ImGui::IsWindowFocused()` 优化不可见窗口

---

## 📞 获取帮助

### 如果遇到问题:

1. **查看文档**: 先查看本目录下的其他文档
2. **运行验证**: 使用 `verify_imgui.ps1 -Detailed` 诊断问题
3. **检查日志**: 查看ImGuiSubsystem的调试输出
4. **查看示例**: 参考 `imgui_demo.cpp` 中的示例代码
5. **联系团队**: 如果问题仍未解决,联系Eurekiel Engine开发团队

---

**版本**: v1.92.4
**更新日期**: 2025-10-15
**状态**: 生产就绪

祝您使用ImGui开发愉快! 🎉
