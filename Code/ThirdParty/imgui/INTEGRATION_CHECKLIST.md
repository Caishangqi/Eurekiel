# ImGui Integration Checklist for Eurekiel Engine

此文件记录ImGui集成到Engine项目的完整步骤和验证清单。

## 状态概览

| 阶段 | 状态 | 说明 |
|-----|------|------|
| 1. 目录创建 | ✅ 完成 | imgui/ 和 backends/ 目录已创建 |
| 2. 文件下载 | ⏳ 待完成 | 需要手动下载ImGui源码 |
| 3. vcxproj更新 | ✅ 完成 | Engine.vcxproj已更新 |
| 4. ImGuiSubsystem更新 | ✅ 完成 | 头文件包含已添加,代码已取消注释 |
| 5. 编译验证 | ⏳ 待完成 | 需要在文件下载后进行 |
| 6. 运行时验证 | ⏳ 待完成 | 需要编译通过后测试 |

---

## 第一步: 下载ImGui源码 (手动操作)

### 下载链接
- **官方Release**: https://github.com/ocornut/imgui/releases/tag/v1.92.4
- **直接下载ZIP**: https://github.com/ocornut/imgui/archive/refs/tags/v1.92.4.zip
- **Git克隆**: `git clone --branch v1.92.4 https://github.com/ocornut/imgui.git`

### 需要复制的文件

**核心文件** (从ImGui根目录复制到 `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\`):

- [ ] `imgui.h`
- [ ] `imgui.cpp`
- [ ] `imgui_draw.cpp`
- [ ] `imgui_tables.cpp`
- [ ] `imgui_widgets.cpp`
- [ ] `imgui_demo.cpp` (可选,但推荐)
- [ ] `imgui_internal.h`
- [ ] `imconfig.h`
- [ ] `imstb_rectpack.h`
- [ ] `imstb_textedit.h`
- [ ] `imstb_truetype.h`

**后端文件** (从 `backends/` 目录复制到 `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\backends\`):

- [ ] `imgui_impl_dx11.h`
- [ ] `imgui_impl_dx11.cpp`
- [ ] `imgui_impl_dx12.h`
- [ ] `imgui_impl_dx12.cpp`
- [ ] `imgui_impl_win32.h`
- [ ] `imgui_impl_win32.cpp`

### 验证文件结构

复制完成后,运行以下命令验证文件结构:

```powershell
# 检查核心文件
dir "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\*.h"
dir "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\*.cpp"

# 检查后端文件
dir "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\backends\*.h"
dir "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\backends\*.cpp"
```

期望看到:
```
核心文件: 11个 (.h和.cpp)
后端文件: 6个 (.h和.cpp)
```

---

## 第二步: 验证vcxproj更新 (自动完成)

以下内容已自动添加到 `F:\p4\Personal\SD\Engine\Code\Engine\Engine.vcxproj`:

### ClCompile部分 (已添加)

```xml
<!-- ImGui Core -->
<ClCompile Include="..\ThirdParty\imgui\imgui.cpp" />
<ClCompile Include="..\ThirdParty\imgui\imgui_draw.cpp" />
<ClCompile Include="..\ThirdParty\imgui\imgui_tables.cpp" />
<ClCompile Include="..\ThirdParty\imgui\imgui_widgets.cpp" />
<ClCompile Include="..\ThirdParty\imgui\imgui_demo.cpp" />
<!-- ImGui Backends -->
<ClCompile Include="..\ThirdParty\imgui\backends\imgui_impl_dx11.cpp" />
<ClCompile Include="..\ThirdParty\imgui\backends\imgui_impl_dx12.cpp" />
<ClCompile Include="..\ThirdParty\imgui\backends\imgui_impl_win32.cpp" />
```

### ClInclude部分 (已添加)

```xml
<!-- ImGui Core Headers -->
<ClInclude Include="..\ThirdParty\imgui\imgui.h" />
<ClInclude Include="..\ThirdParty\imgui\imgui_internal.h" />
<ClInclude Include="..\ThirdParty\imgui\imconfig.h" />
<ClInclude Include="..\ThirdParty\imgui\imstb_rectpack.h" />
<ClInclude Include="..\ThirdParty\imgui\imstb_textedit.h" />
<ClInclude Include="..\ThirdParty\imgui\imstb_truetype.h" />
<!-- ImGui Backend Headers -->
<ClInclude Include="..\ThirdParty\imgui\backends\imgui_impl_dx11.h" />
<ClInclude Include="..\ThirdParty\imgui\backends\imgui_impl_dx12.h" />
<ClInclude Include="..\ThirdParty\imgui\backends\imgui_impl_win32.h" />
```

### 包含路径更新 (已添加)

在所有配置的 `<AdditionalIncludeDirectories>` 中已添加:
```
..\ThirdParty\imgui;..\ThirdParty\imgui\backends;
```

**验证checklist:**
- [ ] ClCompile包含8个ImGui cpp文件
- [ ] ClInclude包含9个ImGui头文件
- [ ] 所有4个配置(Debug/Release x Win32/x64)的包含路径已更新

---

## 第三步: 验证ImGuiSubsystem更新 (自动完成)

`F:\p4\Personal\SD\Engine\Code\Engine\Core\ImGui\ImGuiSubsystem.cpp` 已更新:

### 已添加的头文件包含

```cpp
#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_win32.h>
```

### 已取消注释的代码区域

- [ ] `InitializeImGuiContext()` - ImGui上下文初始化代码
- [ ] `ShutdownImGuiContext()` - ImGui上下文清理代码
- [ ] `BeginFrame()` - 帧开始调用
- [ ] `EndFrame()` - 帧结束调用(多视口支持)
- [ ] `Render()` - 渲染调用

**验证方法:**
打开 `ImGuiSubsystem.cpp` 搜索 `TODO:` 关键字,确认所有ImGui库相关的TODO都已取消注释。

---

## 第四步: 编译验证 (下载文件后执行)

### 编译步骤

1. **清理解决方案**
   ```cmd
   cd "F:\p4\Personal\SD\Engine"
   msbuild Engine.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
   ```

2. **重新编译Engine项目**
   ```cmd
   msbuild Engine.sln /t:Engine /p:Configuration=Debug /p:Platform=x64
   ```

3. **检查编译输出**
   - [ ] 无ImGui相关的编译错误
   - [ ] 无ImGui相关的链接错误
   - [ ] Engine.lib成功生成

### 预期问题和解决方案

| 问题 | 原因 | 解决方案 |
|-----|------|---------|
| `fatal error C1083: Cannot open include file: 'imgui.h'` | ImGui文件未下载 | 完成第一步:下载并复制文件 |
| `LNK2019: unresolved external symbol ImGui::*` | ImGui cpp文件未编译 | 检查vcxproj中的ClCompile条目 |
| `error C2039: 'ImGui_ImplWin32_*' is not a member` | 后端文件缺失 | 检查backends目录是否有文件 |

---

## 第五步: 运行时验证

### 基本功能测试

1. **ImGui上下文创建**
   - [ ] 启动程序时ImGuiSubsystem成功初始化
   - [ ] 日志显示: `[ImGuiSubsystem] Initialized successfully with backend: ...`

2. **ImGui窗口注册**
   ```cpp
   // 在你的测试代码中
   imguiSubsystem->RegisterWindow("TestWindow", []() {
       ImGui::Begin("Test Window");
       ImGui::Text("Hello, ImGui!");
       ImGui::End();
   });
   ```
   - [ ] 窗口成功注册
   - [ ] 窗口在屏幕上显示

3. **ImGui Demo窗口**
   ```cpp
   imguiSubsystem->RegisterWindow("ImGui Demo", []() {
       ImGui::ShowDemoWindow();
   });
   ```
   - [ ] Demo窗口正常显示
   - [ ] 所有Demo功能正常工作

### 性能验证

- [ ] ImGui渲染不影响主渲染管线
- [ ] 无明显帧率下降
- [ ] 无内存泄漏(运行一段时间后内存稳定)

### 功能验证

- [ ] 输入响应正常(鼠标、键盘)
- [ ] 窗口拖拽正常
- [ ] 控件交互正常(按钮、文本框等)
- [ ] 字体渲染正常

---

## 第六步: 集成到EurekielFeatureTest项目

一旦Engine项目编译通过,可以在EurekielFeatureTest中使用ImGui:

### 在App.cpp中添加测试窗口

```cpp
#include "Engine/Core/ImGui/ImGuiSubsystem.hpp"

void App::Startup()
{
    // ... existing code ...

    // 获取ImGuiSubsystem
    auto* imguiSubsystem = g_theSubsystems->GetSubsystem<ImGuiSubsystem>();

    // 注册测试窗口
    if (imguiSubsystem)
    {
        imguiSubsystem->RegisterWindow("App Debug", []() {
            ImGui::Begin("App Debug Window");
            ImGui::Text("Application: EurekielFeatureTest");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            if (ImGui::Button("Test Button"))
            {
                DebuggerPrintf("ImGui button clicked!\n");
            }
            ImGui::End();
        });

        // 注册ImGui Demo窗口(用于学习和测试)
        imguiSubsystem->RegisterWindow("ImGui Demo", []() {
            ImGui::ShowDemoWindow();
        });
    }
}
```

**验证清单:**
- [ ] Debug窗口成功显示
- [ ] FPS信息正确显示
- [ ] 按钮点击触发日志输出
- [ ] Demo窗口功能完整

---

## 常见问题排查

### Q1: 编译时找不到imgui.h

**原因**: ImGui文件未正确复制或包含路径未正确配置

**解决步骤**:
1. 检查文件是否存在: `dir "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\imgui.h"`
2. 检查Engine.vcxproj中的AdditionalIncludeDirectories
3. 重新加载Visual Studio解决方案

### Q2: 链接错误(LNK2019)

**原因**: ImGui cpp文件未包含在编译中

**解决步骤**:
1. 打开Engine.vcxproj,搜索`imgui.cpp`
2. 确认所有8个cpp文件都在ClCompile组中
3. 清理并重新编译

### Q3: ImGui窗口不显示

**原因**: 渲染管线未正确调用ImGui的渲染函数

**解决步骤**:
1. 确认在渲染循环中调用了`BeginFrame()`, `Render()`, `EndFrame()`
2. 确认后端正确初始化
3. 检查ImGuiSubsystem的日志输出

### Q4: Win32后端错误

**原因**: Win32平台层未正确初始化

**解决步骤**:
1. 确认ImGuiSubsystemConfig中的targetWindow不为空
2. 确认调用了`ImGui_ImplWin32_Init(hwnd)`
3. 确认在消息处理中调用了`ImGui_ImplWin32_WndProcHandler`

---

## 下一步开发

ImGui集成完成后,可以进行以下开发:

1. **[Task 1.5] 实现ImGuiBackendDX11**
   - 创建 `Engine/Core/ImGui/ImGuiBackendDX11.hpp`
   - 创建 `Engine/Core/ImGui/ImGuiBackendDX11.cpp`
   - 封装 `imgui_impl_dx11.cpp` 的功能

2. **[Task 1.6] 实现ImGuiBackendDX12**
   - 创建 `Engine/Core/ImGui/ImGuiBackendDX12.hpp`
   - 创建 `Engine/Core/ImGui/ImGuiBackendDX12.cpp`
   - 封装 `imgui_impl_dx12.cpp` 的功能

3. **创建ImGui工具窗口**
   - 性能监控窗口
   - 资源查看器
   - 日志查看器
   - 场景调试器

---

## 最终验证清单

在所有步骤完成后,进行最终验证:

- [ ] Engine项目编译无警告无错误
- [ ] EurekielFeatureTest项目链接Engine.lib成功
- [ ] 程序启动时ImGuiSubsystem成功初始化
- [ ] ImGui Demo窗口完整显示
- [ ] 自定义ImGui窗口正常工作
- [ ] 程序退出时ImGui正确清理
- [ ] 无内存泄漏
- [ ] 性能稳定

---

## 集成完成确认

当上述所有检查项都标记为完成时,ImGui集成即宣告完成。

**集成负责人**: _____________
**完成日期**: _____________
**验证人**: _____________

---

## 参考资料

- ImGui官方文档: https://github.com/ocornut/imgui/wiki
- ImGui FAQ: https://github.com/ocornut/imgui/blob/master/docs/FAQ.md
- Engine项目结构: F:\p4\Personal\SD\Engine\Code\Engine\
- ImGuiSubsystem源码: F:\p4\Personal\SD\Engine\Code\Engine\Core\ImGui\

---

**注意**: 此文档应与代码一起保存在版本控制中,以便团队成员参考和未来更新ImGui时使用。
