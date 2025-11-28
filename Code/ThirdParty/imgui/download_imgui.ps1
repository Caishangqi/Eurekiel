# ImGui v1.92.4 一键下载安装脚本
# 作者: Eurekiel Engine Team
# 日期: 2025-10-15

param(
    [string]$TempDir = "$env:TEMP",
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
        Write-Host "   成功 下载完成: $imguiZip" -ForegroundColor Green
    } catch {
        Write-Host "   失败 下载失败: $_" -ForegroundColor Red
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
    Write-Host "   成功 解压完成: $imguiExtracted" -ForegroundColor Green
} catch {
    Write-Host "   失败 解压失败: $_" -ForegroundColor Red
    exit 1
}

# 步骤3: 创建目标目录
Write-Host "[3/5] 创建目标目录..." -ForegroundColor Yellow

if (-not (Test-Path $destRoot)) {
    New-Item -ItemType Directory -Path $destRoot -Force | Out-Null
    Write-Host "   成功 创建: $destRoot" -ForegroundColor Green
} else {
    Write-Host "   目录已存在: $destRoot" -ForegroundColor Gray
}

if (-not (Test-Path $destBackends)) {
    New-Item -ItemType Directory -Path $destBackends -Force | Out-Null
    Write-Host "   成功 创建: $destBackends" -ForegroundColor Green
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
$coreSuccess = 0
foreach ($file in $coreFiles) {
    $src = Join-Path $imguiExtracted $file
    $dst = Join-Path $destRoot $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "      成功 $file" -ForegroundColor Green
        $coreSuccess++
    } else {
        Write-Host "      失败 未找到: $file" -ForegroundColor Red
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
$backendSuccess = 0
foreach ($file in $backendFiles) {
    $src = Join-Path "$imguiExtracted\backends" $file
    $dst = Join-Path $destBackends $file

    if (Test-Path $src) {
        Copy-Item -Path $src -Destination $dst -Force
        Write-Host "      成功 $file" -ForegroundColor Green
        $backendSuccess++
    } else {
        Write-Host "      失败 未找到: $file" -ForegroundColor Red
    }
}

# 步骤5: 验证安装
Write-Host "[5/5] 验证安装..." -ForegroundColor Yellow

Write-Host "   核心文件: $coreSuccess / 11" -ForegroundColor $(if($coreSuccess -eq 11){"Green"}else{"Red"})
Write-Host "   后端文件: $backendSuccess / 6" -ForegroundColor $(if($backendSuccess -eq 6){"Green"}else{"Red"})

# 清理临时文件
if ($CleanTemp) {
    Write-Host ""
    Write-Host "清理临时文件..." -ForegroundColor Yellow

    if (Test-Path $imguiZip) {
        Remove-Item -Path $imguiZip -Force
        Write-Host "   成功 删除: $imguiZip" -ForegroundColor Green
    }

    if (Test-Path $imguiExtracted) {
        Remove-Item -Path $imguiExtracted -Recurse -Force
        Write-Host "   成功 删除: $imguiExtracted" -ForegroundColor Green
    }
}

# 最终结果
Write-Host ""
Write-Host "==================================================" -ForegroundColor Cyan

if ($coreSuccess -eq 11 -and $backendSuccess -eq 6) {
    Write-Host " 成功 ImGui v$imguiVersion 安装成功!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "下一步:" -ForegroundColor Yellow
    Write-Host "1. 打开 Visual Studio 2022" -ForegroundColor White
    Write-Host "2. 重新加载 Engine.sln 解决方案" -ForegroundColor White
    Write-Host "3. 编译 Engine 项目 (Debug x64)" -ForegroundColor White
    Write-Host "4. 查看 INTEGRATION_CHECKLIST.md 完成后续验证" -ForegroundColor White
    exit 0
} else {
    Write-Host " 失败 ImGui 安装不完整!" -ForegroundColor Red
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "请检查错误信息并重试,或手动复制缺失的文件" -ForegroundColor Yellow
    exit 1
}

Write-Host ""
