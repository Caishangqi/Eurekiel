# ImGui集成验证脚本
# 用于检查ImGui文件是否已正确下载和配置

param(
    [switch]$Detailed
)

$ErrorActionPreference = "Continue"

# 定义路径
$imguiRoot = "F:\p4\Personal\SD\Engine\Code\ThirdParty\imgui"
$backendDir = "$imguiRoot\backends"
$vcxproj = "F:\p4\Personal\SD\Engine\Code\Engine\Engine.vcxproj"

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " ImGui集成验证脚本" -ForegroundColor Cyan
Write-Host " 版本: v1.92.4" -ForegroundColor Cyan
Write-Host " 日期: 2025-10-15" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# 统计变量
$totalChecks = 0
$passedChecks = 0
$failedChecks = 0

function Test-FileExists {
    param(
        [string]$Path,
        [string]$Description
    )

    $script:totalChecks++

    if (Test-Path $Path) {
        Write-Host "   ✓ " -NoNewline -ForegroundColor Green
        Write-Host $Description -ForegroundColor Gray
        $script:passedChecks++
        return $true
    } else {
        Write-Host "   ✗ " -NoNewline -ForegroundColor Red
        Write-Host $Description -ForegroundColor Gray
        if ($Detailed) {
            Write-Host "      路径: $Path" -ForegroundColor DarkGray
        }
        $script:failedChecks++
        return $false
    }
}

function Test-StringInFile {
    param(
        [string]$FilePath,
        [string]$SearchString,
        [string]$Description
    )

    $script:totalChecks++

    if (-not (Test-Path $FilePath)) {
        Write-Host "   ✗ " -NoNewline -ForegroundColor Red
        Write-Host "$Description (文件不存在)" -ForegroundColor Gray
        $script:failedChecks++
        return $false
    }

    $content = Get-Content -Path $FilePath -Raw
    if ($content -match [regex]::Escape($SearchString)) {
        Write-Host "   ✓ " -NoNewline -ForegroundColor Green
        Write-Host $Description -ForegroundColor Gray
        $script:passedChecks++
        return $true
    } else {
        Write-Host "   ✗ " -NoNewline -ForegroundColor Red
        Write-Host $Description -ForegroundColor Gray
        if ($Detailed) {
            Write-Host "      未找到: $SearchString" -ForegroundColor DarkGray
        }
        $script:failedChecks++
        return $false
    }
}

# ====================
# 阶段1: ��录结构检查
# ====================
Write-Host "[阶段 1/5] 检查目录结构" -ForegroundColor Yellow
Write-Host ""

Test-FileExists -Path $imguiRoot -Description "ImGui根目录存在"
Test-FileExists -Path $backendDir -Description "backends子目录存在"

Write-Host ""

# ====================
# 阶段2: 核心文件检查
# ====================
Write-Host "[阶段 2/5] 检查核心文件 (11个)" -ForegroundColor Yellow
Write-Host ""

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

foreach ($file in $coreFiles) {
    Test-FileExists -Path "$imguiRoot\$file" -Description $file
}

Write-Host ""

# ====================
# 阶段3: 后端文件检查
# ====================
Write-Host "[阶段 3/5] 检查后端文件 (6个)" -ForegroundColor Yellow
Write-Host ""

$backendFiles = @(
    "imgui_impl_dx11.h",
    "imgui_impl_dx11.cpp",
    "imgui_impl_dx12.h",
    "imgui_impl_dx12.cpp",
    "imgui_impl_win32.h",
    "imgui_impl_win32.cpp"
)

foreach ($file in $backendFiles) {
    Test-FileExists -Path "$backendDir\$file" -Description $file
}

Write-Host ""

# ====================
# 阶段4: vcxproj配置检查
# ====================
Write-Host "[阶段 4/5] 检查Engine.vcxproj配置" -ForegroundColor Yellow
Write-Host ""

# 检查vcxproj文件是否存在
if (Test-Path $vcxproj) {
    Write-Host "   检查ClCompile条目..." -ForegroundColor Gray

    # 核心cpp文件
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui.cpp' -Description "imgui.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui_draw.cpp' -Description "imgui_draw.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui_tables.cpp' -Description "imgui_tables.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui_widgets.cpp' -Description "imgui_widgets.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui_demo.cpp' -Description "imgui_demo.cpp已添加"

    # 后端cpp文件
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends\imgui_impl_dx11.cpp' -Description "imgui_impl_dx11.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends\imgui_impl_dx12.cpp' -Description "imgui_impl_dx12.cpp已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends\imgui_impl_win32.cpp' -Description "imgui_impl_win32.cpp已添加"

    Write-Host ""
    Write-Host "   检查ClInclude条目..." -ForegroundColor Gray

    # 核心头文件
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui.h' -Description "imgui.h已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\imgui_internal.h' -Description "imgui_internal.h已添加"

    # 后端头文件
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends\imgui_impl_dx11.h' -Description "imgui_impl_dx11.h已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends\imgui_impl_win32.h' -Description "imgui_impl_win32.h已添加"

    Write-Host ""
    Write-Host "   检查包含路径..." -ForegroundColor Gray

    # 包含路径
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui' -Description "imgui包含路径已添加"
    Test-StringInFile -FilePath $vcxproj -SearchString '..\ThirdParty\imgui\backends' -Description "backends包含路径已添加"
} else {
    Write-Host "   ✗ " -NoNewline -ForegroundColor Red
    Write-Host "Engine.vcxproj文件不存在!" -ForegroundColor Red
    $script:failedChecks += 14
    $script:totalChecks += 14
}

Write-Host ""

# ====================
# 阶段5: ImGuiSubsystem代码检查
# ====================
Write-Host "[阶段 5/5] 检查ImGuiSubsystem.cpp代码" -ForegroundColor Yellow
Write-Host ""

$imguiSubsystemCpp = "F:\p4\Personal\SD\Engine\Code\Engine\Core\ImGui\ImGuiSubsystem.cpp"

if (Test-Path $imguiSubsystemCpp) {
    Write-Host "   检查头文件包含..." -ForegroundColor Gray

    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString '#include <imgui.h>' -Description "#include <imgui.h>"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString '#include <imgui_internal.h>' -Description "#include <imgui_internal.h>"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString '#include <backends/imgui_impl_win32.h>' -Description "#include <backends/imgui_impl_win32.h>"

    Write-Host ""
    Write-Host "   检查ImGui API调用..." -ForegroundColor Gray

    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'IMGUI_CHECKVERSION()' -Description "IMGUI_CHECKVERSION()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui::CreateContext()' -Description "ImGui::CreateContext()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui::NewFrame()' -Description "ImGui::NewFrame()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui::Render()' -Description "ImGui::Render()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui::DestroyContext()' -Description "ImGui::DestroyContext()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui_ImplWin32_Init' -Description "ImGui_ImplWin32_Init()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui_ImplWin32_NewFrame' -Description "ImGui_ImplWin32_NewFrame()已取消注释"
    Test-StringInFile -FilePath $imguiSubsystemCpp -SearchString 'ImGui_ImplWin32_Shutdown' -Description "ImGui_ImplWin32_Shutdown()已取消注释"
} else {
    Write-Host "   ✗ " -NoNewline -ForegroundColor Red
    Write-Host "ImGuiSubsystem.cpp文件不存在!" -ForegroundColor Red
    $script:failedChecks += 11
    $script:totalChecks += 11
}

Write-Host ""

# ====================
# 汇总报告
# ====================
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host " 验证结果汇总" -ForegroundColor Cyan
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

$passRate = if ($totalChecks -gt 0) { [math]::Round(($passedChecks / $totalChecks) * 100, 2) } else { 0 }

Write-Host "总检查项: $totalChecks" -ForegroundColor White
Write-Host "通过: $passedChecks" -ForegroundColor Green
Write-Host "失败: $failedChecks" -ForegroundColor Red
Write-Host "通过率: $passRate%" -ForegroundColor $(if($passRate -eq 100){"Green"}elseif($passRate -ge 80){"Yellow"}else{"Red"})

Write-Host ""

# 判断整体状态
if ($failedChecks -eq 0) {
    Write-Host "✓ ImGui集成验证通过!" -ForegroundColor Green
    Write-Host ""
    Write-Host "下一步:" -ForegroundColor Yellow
    Write-Host "1. 在Visual Studio中重新加载Engine.sln" -ForegroundColor White
    Write-Host "2. 编译Engine项目 (Debug x64)" -ForegroundColor White
    Write-Host "3. 检查编译输出,确保无ImGui相关错误" -ForegroundColor White
    Write-Host "4. 参考INTEGRATION_CHECKLIST.md完成运行时验证" -ForegroundColor White
} elseif ($passRate -ge 80) {
    Write-Host "⚠ ImGui集成基本完成,但有一些问题需要解决" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "建议:" -ForegroundColor Yellow
    Write-Host "1. 检查上述失败的项目" -ForegroundColor White
    Write-Host "2. 参考DOWNLOAD_GUIDE.md重新下载缺失的文件" -ForegroundColor White
    Write-Host "3. 重新运行此脚本验证" -ForegroundColor White
} else {
    Write-Host "✗ ImGui集成失败或不完整!" -ForegroundColor Red
    Write-Host ""
    Write-Host "建议:" -ForegroundColor Yellow
    Write-Host "1. 参考DOWNLOAD_GUIDE.md从头开始" -ForegroundColor White
    Write-Host "2. 确保所有文件都已正确下载" -ForegroundColor White
    Write-Host "3. 检查Engine.vcxproj是否已更新" -ForegroundColor White
    Write-Host "4. 使用 -Detailed 参数查看详细错误信息" -ForegroundColor White
}

Write-Host ""
Write-Host "================================================================" -ForegroundColor Cyan
Write-Host ""

# 返回退出码
if ($failedChecks -eq 0) {
    exit 0
} elseif ($passRate -ge 80) {
    exit 1
} else {
    exit 2
}
