# ImGui v1.92.4 下载指南

本文档提供详细的步骤指导如何下载并安装ImGui v1.92.4到Eurekiel Engine项目。

---

## 快速开始

### 步骤1: 下载ImGui源码

**方法A: 直接下载ZIP文件(推荐)**

1. 访问以下链接:
   ```
   https://github.com/ocornut/imgui/archive/refs/tags/v1.92.4.zip
   ```

2. 浏览器会自动下载 `imgui-1.92.4.zip` 文件

3. 解压ZIP文件到临时文件夹(例如: `C:\Temp\imgui-1.92.4\`)

**方法B: 使用Git克隆**

```bash
git clone --branch v1.92.4 --depth 1 https://github.com/ocornut/imgui.git C:\Temp\imgui-1.92.4
```

**方法C: 使用GitHub Release页面下载**

1. 访问: https://github.com/ocornut/imgui/releases/tag/v1.92.4
2. 点击 "Source code (zip)" 下载
3. 解压到临时文件夹

---

## 步骤2: 复制核心文件

**目标目录**: `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\`

从 `C:\Temp\imgui-1.92.4\` 复制以下文件到目标目录:

```
✓ imgui.h
✓ imgui.cpp
✓ imgui_draw.cpp
✓ imgui_tables.cpp
✓ imgui_widgets.cpp
✓ imgui_demo.cpp
✓ imgui_internal.h
✓ imconfig.h
✓ imstb_rectpack.h
✓ imstb_textedit.h
✓ imstb_truetype.h
```

**PowerShell命令** (一键复制):

```powershell
# 设置源目录和目标目录
$source = "C:\Temp\imgui-1.92.4"
$dest = "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui"

# 复制核心文件
Copy-Item "$source\imgui.h" -Destination $dest
Copy-Item "$source\imgui.cpp" -Destination $dest
Copy-Item "$source\imgui_draw.cpp" -Destination $dest
Copy-Item "$source\imgui_tables.cpp" -Destination $dest
Copy-Item "$source\imgui_widgets.cpp" -Destination $dest
Copy-Item "$source\imgui_demo.cpp" -Destination $dest
Copy-Item "$source\imgui_internal.h" -Destination $dest
Copy-Item "$source\imconfig.h" -Destination $dest
Copy-Item "$source\imstb_rectpack.h" -Destination $dest
Copy-Item "$source\imstb_textedit.h" -Destination $dest
Copy-Item "$source\imstb_truetype.h" -Destination $dest

Write-Host "核心文件复制完成!" -ForegroundColor Green
```

---

## 步骤3: 复制后端文件

**目标目录**: `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\backends\`

从 `C:\Temp\imgui-1.92.4\backends\` 复制以下文件到目标目录:

```
✓ imgui_impl_dx11.h
✓ imgui_impl_dx11.cpp
✓ imgui_impl_dx12.h
✓ imgui_impl_dx12.cpp
✓ imgui_impl_win32.h
✓ imgui_impl_win32.cpp
```

**PowerShell命令** (一键复制):

```powershell
# 设置源目录和目标目录
$source = "C:\Temp\imgui-1.92.4\backends"
$dest = "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\backends"

# 确保backends目录存在
if (-not (Test-Path $dest)) {
    New-Item -ItemType Directory -Path $dest -Force
}

# 复制后端文件
Copy-Item "$source\imgui_impl_dx11.h" -Destination $dest
Copy-Item "$source\imgui_impl_dx11.cpp" -Destination $dest
Copy-Item "$source\imgui_impl_dx12.h" -Destination $dest
Copy-Item "$source\imgui_impl_dx12.cpp" -Destination $dest
Copy-Item "$source\imgui_impl_win32.h" -Destination $dest
Copy-Item "$source\imgui_impl_win32.cpp" -Destination $dest

Write-Host "后端文件复制完成!" -ForegroundColor Green
```

---

## 步骤4: 验证文件结构

运行以下命令验证所有文件都已正确复制:

```powershell
$imgui = "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui"

Write-Host "`n=== 检查核心文件 ===" -ForegroundColor Cyan
Get-ChildItem -Path $imgui -Filter "*.h" | Select-Object Name
Get-ChildItem -Path $imgui -Filter "*.cpp" | Select-Object Name

Write-Host "`n=== 检查后端文件 ===" -ForegroundColor Cyan
Get-ChildItem -Path "$imgui\backends" -Filter "*.h" | Select-Object Name
Get-ChildItem -Path "$imgui\backends" -Filter "*.cpp" | Select-Object Name

Write-Host "`n=== 文件数量统计 ===" -ForegroundColor Cyan
$coreFiles = (Get-ChildItem -Path $imgui -Filter "*.h").Count + (Get-ChildItem -Path $imgui -Filter "*.cpp").Count
$backendFiles = (Get-ChildItem -Path "$imgui\backends" -Filter "*impl*").Count
Write-Host "核心文件数量: $coreFiles (期望: 11)" -ForegroundColor $(if($coreFiles -eq 11){"Green"}else{"Red"})
Write-Host "后端文件数量: $backendFiles (期望: 6)" -ForegroundColor $(if($backendFiles -eq 6){"Green"}else{"Red"})

if ($coreFiles -eq 11 -and $backendFiles -eq 6) {
    Write-Host "`n✓ 所有文件已正确复制!" -ForegroundColor Green
} else {
    Write-Host "`n✗ 文件不完整,请检查!" -ForegroundColor Red
}
```

**期望输出**:

```
=== 检查核心文件 ===
imconfig.h
imgui.h
imgui_internal.h
imstb_rectpack.h
imstb_textedit.h
imstb_truetype.h
imgui.cpp
imgui_demo.cpp
imgui_draw.cpp
imgui_tables.cpp
imgui_widgets.cpp

=== 检查后端文件 ===
imgui_impl_dx11.h
imgui_impl_dx12.h
imgui_impl_win32.h
imgui_impl_dx11.cpp
imgui_impl_dx12.cpp
imgui_impl_win32.cpp

=== 文件数量统计 ===
核心文件数量: 11 (期望: 11)
后端文件数量: 6 (期望: 6)

✓ 所有文件已正确复制!
```

---

## 步骤5: 一键完整脚本

如果您想一次性完成所有步骤,可以使用以下完整脚本:

**保存为**: `F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui\download_imgui.ps1`

```powershell
# ImGui v1.92.4 一键下载安装脚本
# 作者: Eurekiel Engine Team
# 日期: 2025-10-15

param(
    [string]$TempDir = "C:\Temp",
    [switch]$SkipDownload,
    [switch]$CleanTemp
)

# 设置严格模式
$ErrorActionPreference = "Stop"

# 定义路径
$imguiVersion = "1.92.4"
$imguiZip = "$TempDir\imgui-$imguiVersion.zip"
$imguiExtracted = "$TempDir\imgui-$imguiVersion"
$destRoot = "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui"
$destBackends = "$destRoot\backends"

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host " ImGui v$imguiVersion 自动安装脚本" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

# 步骤1: 下载ImGui
if (-not $SkipDownload) {
    Write-Host "[1/5] 下载ImGui v$imguiVersion..." -ForegroundColor Yellow

    # 确保临时目录存在
    if (-not (Test-Path $TempDir)) {
        New-Item -ItemType Directory -Path $TempDir -Force | Out-Null
    }

    # 下载ZIP文件
    $downloadUrl = "https://github.com/ocornut/imgui/archive/refs/tags/v$imguiVersion.zip"
    Write-Host "   下载地址: $downloadUrl" -ForegroundColor Gray

    try {
        Invoke-WebRequest -Uri $downloadUrl -OutFile $imguiZip -UseBasicParsing
        Write-Host "   ✓ 下载完成: $imguiZip" -ForegroundColor Green
    } catch {
        Write-Host "   ✗ 下载失败: $_" -ForegroundColor Red
        Write-Host ""
        Write-Host "请手动下载并解压,然后使用 -SkipDownload 参数重新运行脚本" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "[1/5] 跳过下载步骤(使用 -SkipDownload)" -ForegroundColor Gray
}

# 步骤2: 解压文件
Write-Host "[2/5] 解压ImGui源码..." -ForegroundColor Yellow

if (Test-Path $imguiExtracted) {
    Write-Host "   清理旧的解压目录..." -ForegroundColor Gray
    Remove-Item -Path $imguiExtracted -Recurse -Force
}

try {
    Expand-Archive -Path $imguiZip -DestinationPath $TempDir -Force
    Write-Host "   ✓ 解压完成: $imguiExtracted" -ForegroundColor Green
} catch {
    Write-Host "   ✗ 解压失败: $_" -ForegroundColor Red
    exit 1
}

# 步骤3: 创建目标目录
Write-Host "[3/5] 创建目标目录..." -ForegroundColor Yellow

if (-not (Test-Path $destRoot)) {
    New-Item -ItemType Directory -Path $destRoot -Force | Out-Null
    Write-Host "   ✓ 创建: $destRoot" -ForegroundColor Green
} else {
    Write-Host "   目录已存在: $destRoot" -ForegroundColor Gray
}

if (-not (Test-Path $destBackends)) {
    New-Item -ItemType Directory -Path $destBackends -Force | Out-Null
    Write-Host "   ✓ 创建: $destBackends" -ForegroundColor Green
} else {
    Write-Host "   目录已存在: $destBackends" -ForegroundColor Gray
}

# 步骤4: 复制文件
Write-Host "[4/5] 复制ImGui文件..." -ForegroundColor Yellow

# 核心文件列表
$coreFiles = @(
    "imgui.h",
    "imgui.cpp",
    "imgui_draw.cpp",
    "imgui_tables.cpp",
    "imgui_widgets.cpp",
    "imgui_demo.cpp",
    "imgui_internal.h",
    "imconfig.h",
    "imstb_rectpack.h",
    "imstb_textedit.h",
    "imstb_truetype.h"
)

Write-Host "   复制核心文件..." -ForegroundColor Gray
foreach ($file in $coreFiles) {
    $src = Join-Path $imguiExtracted $file
    $dst = Join-Path $destRoot $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "      ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "      ✗ 未找到: $file" -ForegroundColor Red
    }
}

# 后端文件列表
$backendFiles = @(
    "imgui_impl_dx11.h",
    "imgui_impl_dx11.cpp",
    "imgui_impl_dx12.h",
    "imgui_impl_dx12.cpp",
    "imgui_impl_win32.h",
    "imgui_impl_win32.cpp"
)

Write-Host "   复制后端文件..." -ForegroundColor Gray
foreach ($file in $backendFiles) {
    $src = Join-Path "$imguiExtracted\backends" $file
    $dst = Join-Path $destBackends $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "      ✓ $file" -ForegroundColor Green
    } else {
        Write-Host "      ✗ 未找到: $file" -ForegroundColor Red
    }
}

# 步骤5: 验证安装
Write-Host "[5/5] 验证安装..." -ForegroundColor Yellow

$coreCount = (Get-ChildItem -Path $destRoot -Filter "*.h").Count + (Get-ChildItem -Path $destRoot -Filter "*.cpp").Count
$backendCount = (Get-ChildItem -Path $destBackends -Filter "*impl*").Count

Write-Host "   核心文件: $coreCount / 11" -ForegroundColor $(if($coreCount -eq 11){"Green"}else{"Red"})
Write-Host "   后端文件: $backendCount / 6" -ForegroundColor $(if($backendCount -eq 6){"Green"}else{"Red"})

# 清理临时文件
if ($CleanTemp) {
    Write-Host ""
    Write-Host "清理临时文件..." -ForegroundColor Yellow

    if (Test-Path $imguiZip) {
        Remove-Item -Path $imguiZip -Force
        Write-Host "   ✓ 删除: $imguiZip" -ForegroundColor Green
    }

    if (Test-Path $imguiExtracted) {
        Remove-Item -Path $imguiExtracted -Recurse -Force
        Write-Host "   ✓ 删除: $imguiExtracted" -ForegroundColor Green
    }
}

# 最终结果
Write-Host ""
Write-Host "==================================================" -ForegroundColor Cyan

if ($coreCount -eq 11 -and $backendCount -eq 6) {
    Write-Host " ✓ ImGui v$imguiVersion 安装成功!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "下一步:" -ForegroundColor Yellow
    Write-Host "1. 打开 Visual Studio 2022" -ForegroundColor White
    Write-Host "2. 重新加载 Engine.sln 解决方案" -ForegroundColor White
    Write-Host "3. 编译 Engine 项目 (Debug x64)" -ForegroundColor White
    Write-Host "4. 查看 INTEGRATION_CHECKLIST.md 完成后续验证" -ForegroundColor White
} else {
    Write-Host " ✗ ImGui 安装不完整!" -ForegroundColor Red
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "请检查错误信息并重试,或手动复制缺失的文件" -ForegroundColor Yellow
}

Write-Host ""
```

**使用方法**:

```powershell
# 基本用法(自动下载+安装)
.\download_imgui.ps1

# 跳过下载(如果已手动下载)
.\download_imgui.ps1 -SkipDownload

# 自动清理临时文件
.\download_imgui.ps1 -CleanTemp

# 指定自定义临时目录
.\download_imgui.ps1 -TempDir "D:\Downloads"
```

---

## 故障排查

### 问题1: "无法下载文件"

**解决方案**:
1. 检查网络连接
2. 尝试使用VPN或镜像站点
3. 手动下载ZIP文件后使用 `-SkipDownload` 参数

### 问题2: "权限被拒绝"

**解决方案**:
以管理员身份运行PowerShell:
```powershell
Start-Process powershell -Verb runAs
```

### 问题3: "执行策略限制"

**解决方案**:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### 问题4: "文件数量不匹配"

**解决方案**:
1. 删除现有文件: `Remove-Item "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui" -Recurse -Force`
2. 重新运行安装脚本

---

## 手动复制清单

如果自动化脚本失败,可以按以下清单手动复制:

### 核心文件 (11个)

- [ ] `imgui.h`
- [ ] `imgui.cpp`
- [ ] `imgui_draw.cpp`
- [ ] `imgui_tables.cpp`
- [ ] `imgui_widgets.cpp`
- [ ] `imgui_demo.cpp`
- [ ] `imgui_internal.h`
- [ ] `imconfig.h`
- [ ] `imstb_rectpack.h`
- [ ] `imstb_textedit.h`
- [ ] `imstb_truetype.h`

### 后端文件 (6个)

- [ ] `backends/imgui_impl_dx11.h`
- [ ] `backends/imgui_impl_dx11.cpp`
- [ ] `backends/imgui_impl_dx12.h`
- [ ] `backends/imgui_impl_dx12.cpp`
- [ ] `backends/imgui_impl_win32.h`
- [ ] `backends/imgui_impl_win32.cpp`

---

## 验证完成后

文件复制完成后,请参考以下文档:

1. **INTEGRATION_CHECKLIST.md** - 完整的集成验证清单
2. **README.md** - ImGui库的详细信息和文档链接
3. **编译Engine项目** - 确保ImGui正确集成

---

## 技术支持

如果遇到任何问题:

1. 查看 `INTEGRATION_CHECKLIST.md` 中的"常见问题排查"章节
2. 检查ImGui官方文档: https://github.com/ocornut/imgui/wiki
3. 联系Eurekiel Engine开发团队

---

**集成日期**: 2025-10-15
**ImGui版本**: v1.92.4
**Engine版本**: Eurekiel Engine (Development Build)
