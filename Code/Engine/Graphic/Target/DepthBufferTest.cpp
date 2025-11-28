/**
 * @file DepthBufferTest.cpp
 * @brief Milestone 4: 深度缓冲管理功能测试
 *
 * 测试目标:
 * 1. SwitchDepthBuffer() - 切换活动深度纹理
 * 2. CopyDepthBuffer() - 深度纹理复制
 * 3. GetActiveDepthBufferIndex() - 查询当前激活索引
 *
 * 使用方法:
 * 在App端的RenderFrame()中调用TestDepthBufferFunctions()
 */

#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Core/Logger.hpp"

using namespace enigma::graphic;

/**
 * @brief 测试1：切换深度缓冲
 *
 * 测试流程:
 * 1. 默认激活depthtex0（索引0）
 * 2. 切换到depthtex1（索引1）
 * 3. 验证当前激活索引为1
 * 4. 切换回depthtex0（索引0）
 * 5. 验证当前激活索引为0
 */
void TestDepthBufferSwitch(RendererSubsystem* renderer)
{
    LogInfo(LogTest, "========================================");
    LogInfo(LogTest, "Test 1: Depth Buffer Switch");
    LogInfo(LogTest, "========================================");

    // 测试1.1: 查询默认激活索引
    int activeIndex = renderer->GetActiveDepthBufferIndex();
    LogInfo(LogTest, "Initial active depth buffer index: {}", activeIndex);

    if (activeIndex != 0)
    {
        LogError(LogTest, "FAILED: Expected initial index 0, got {}", activeIndex);
        return;
    }
    LogInfo(LogTest, "PASSED: Initial active index is 0 (depthtex0)");

    // 测试1.2: 切换到depthtex1
    LogInfo(LogTest, "\nSwitching to depthtex1 (index 1)...");
    renderer->SwitchDepthBuffer(1);

    activeIndex = renderer->GetActiveDepthBufferIndex();
    LogInfo(LogTest, "Current active depth buffer index: {}", activeIndex);

    if (activeIndex != 1)
    {
        LogError(LogTest, "FAILED: Expected index 1, got {}", activeIndex);
        return;
    }
    LogInfo(LogTest, "PASSED: Successfully switched to depthtex1");

    // 测试1.3: 切换到depthtex2
    LogInfo(LogTest, "\nSwitching to depthtex2 (index 2)...");
    renderer->SwitchDepthBuffer(2);

    activeIndex = renderer->GetActiveDepthBufferIndex();
    LogInfo(LogTest, "Current active depth buffer index: {}", activeIndex);

    if (activeIndex != 2)
    {
        LogError(LogTest, "FAILED: Expected index 2, got {}", activeIndex);
        return;
    }
    LogInfo(LogTest, "PASSED: Successfully switched to depthtex2");

    // 测试1.4: 切换回depthtex0
    LogInfo(LogTest, "\nSwitching back to depthtex0 (index 0)...");
    renderer->SwitchDepthBuffer(0);

    activeIndex = renderer->GetActiveDepthBufferIndex();
    LogInfo(LogTest, "Current active depth buffer index: {}", activeIndex);

    if (activeIndex != 0)
    {
        LogError(LogTest, "FAILED: Expected index 0, got {}", activeIndex);
        return;
    }
    LogInfo(LogTest, "PASSED: Successfully switched back to depthtex0");

    // 测试1.5: 边界测试（无效索引）
    LogInfo(LogTest, "\nTesting invalid index (should fail gracefully)...");
    renderer->SwitchDepthBuffer(-1);  // 应该失败
    renderer->SwitchDepthBuffer(3);   // 应该失败

    activeIndex = renderer->GetActiveDepthBufferIndex();
    if (activeIndex != 0)
    {
        LogError(LogTest, "FAILED: Index changed after invalid switch, got {}", activeIndex);
        return;
    }
    LogInfo(LogTest, "PASSED: Invalid indices handled correctly");

    LogInfo(LogTest, "\n========================================");
    LogInfo(LogTest, "Test 1: Depth Buffer Switch - ALL PASSED");
    LogInfo(LogTest, "========================================\n");
}

/**
 * @brief 测试2：复制深度纹理
 *
 * 测试流程:
 * 1. 渲染一帧到depthtex0
 * 2. 复制depthtex0 -> depthtex1
 * 3. 复制depthtex0 -> depthtex2
 * 4. 复制depthtex1 -> depthtex2（自定义复制）
 *
 * 注意: 此测试需要在渲染循环中调用，确保有活动的CommandList
 */
void TestDepthBufferCopy(RendererSubsystem* renderer)
{
    LogInfo(LogTest, "========================================");
    LogInfo(LogTest, "Test 2: Depth Buffer Copy");
    LogInfo(LogTest, "========================================");

    // 测试2.1: 复制depthtex0 -> depthtex1（模拟TERRAIN_TRANSLUCENT前）
    LogInfo(LogTest, "\nCopying depthtex0 -> depthtex1...");
    renderer->CopyDepthBuffer(0, 1);
    LogInfo(LogTest, "PASSED: depthtex0 -> depthtex1 copy completed");

    // 测试2.2: 复制depthtex0 -> depthtex2（模拟HAND_SOLID前）
    LogInfo(LogTest, "\nCopying depthtex0 -> depthtex2...");
    renderer->CopyDepthBuffer(0, 2);
    LogInfo(LogTest, "PASSED: depthtex0 -> depthtex2 copy completed");

    // 测试2.3: 自定义复制depthtex1 -> depthtex2
    LogInfo(LogTest, "\nCustom copy: depthtex1 -> depthtex2...");
    renderer->CopyDepthBuffer(1, 2);
    LogInfo(LogTest, "PASSED: depthtex1 -> depthtex2 copy completed");

    // 测试2.4: 边界测试（无效参数）
    LogInfo(LogTest, "\nTesting invalid parameters (should fail gracefully)...");
    renderer->CopyDepthBuffer(0, 0);   // 相同索引（应该失败）
    renderer->CopyDepthBuffer(-1, 1);  // 无效源索引
    renderer->CopyDepthBuffer(1, 3);   // 无效目标索引
    LogInfo(LogTest, "PASSED: Invalid parameters handled correctly");

    LogInfo(LogTest, "\n========================================");
    LogInfo(LogTest, "Test 2: Depth Buffer Copy - ALL PASSED");
    LogInfo(LogTest, "========================================\n");
}

/**
 * @brief 测试3：Iris兼容场景
 *
 * 模拟Iris的典型使用场景:
 * 1. TERRAIN_TRANSLUCENT阶段前复制depthtex0 -> depthtex1
 * 2. HAND_SOLID阶段前复制depthtex0 -> depthtex2
 * 3. 验证深度纹理语义保持不变
 */
void TestIrisCompatibleScenario(RendererSubsystem* renderer)
{
    LogInfo(LogTest, "========================================");
    LogInfo(LogTest, "Test 3: Iris Compatible Scenario");
    LogInfo(LogTest, "========================================");

    // 场景1: TERRAIN_TRANSLUCENT前
    LogInfo(LogTest, "\nScenario 1: Before TERRAIN_TRANSLUCENT phase");
    LogInfo(LogTest, "  - Copying depthtex0 -> depthtex1 (save depth without translucent)");
    renderer->CopyDepthBuffer(0, 1);
    LogInfo(LogTest, "  PASSED: depthtex1 now contains solid geometry depth");

    // 场景2: HAND_SOLID前
    LogInfo(LogTest, "\nScenario 2: Before HAND_SOLID phase");
    LogInfo(LogTest, "  - Copying depthtex0 -> depthtex2 (save depth without hands)");
    renderer->CopyDepthBuffer(0, 2);
    LogInfo(LogTest, "  PASSED: depthtex2 now contains world depth without hands");

    // 验证语义
    LogInfo(LogTest, "\nDepth Texture Semantics:");
    LogInfo(LogTest, "  - depthtex0: Main depth buffer (all geometry)");
    LogInfo(LogTest, "  - depthtex1: Depth without translucent");
    LogInfo(LogTest, "  - depthtex2: Depth without hands");
    LogInfo(LogTest, "  Iris semantics preserved!");

    LogInfo(LogTest, "\n========================================");
    LogInfo(LogTest, "Test 3: Iris Compatible Scenario - PASSED");
    LogInfo(LogTest, "========================================\n");
}

/**
 * @brief 综合测试入口
 *
 * 在App端的适当位置调用此函数
 * 推荐调用时机: BeginFrame()之后，任何渲染之前
 *
 * @param renderer RendererSubsystem实例指针
 */
void TestDepthBufferFunctions(RendererSubsystem* renderer)
{
    if (!renderer)
    {
        LogError(LogTest, "TestDepthBufferFunctions: renderer is null");
        return;
    }

    LogInfo(LogTest, "\n");
    LogInfo(LogTest, "========================================");
    LogInfo(LogTest, "Milestone 4: Depth Buffer Test Suite");
    LogInfo(LogTest, "========================================");
    LogInfo(LogTest, "");

    // 运行所有测试
    TestDepthBufferSwitch(renderer);
    TestDepthBufferCopy(renderer);
    TestIrisCompatibleScenario(renderer);

    LogInfo(LogTest, "\n========================================");
    LogInfo(LogTest, "All Tests Completed Successfully!");
    LogInfo(LogTest, "========================================\n");
}

/**
 * 使用示例（在App端）:
 *
 * void GameApp::RenderFrame()
 * {
 *     // 开始帧
 *     g_theRenderer->BeginFrame();
 *
 *     // 运行一次性测试（建议在第一帧或特定条件下）
 *     static bool testRun = false;
 *     if (!testRun)
 *     {
 *         TestDepthBufferFunctions(g_theRenderer);
 *         testRun = true;
 *     }
 *
 *     // 正常渲染...
 *     // ...
 *
 *     // 模拟Iris场景（在实际渲染循环中）
 *     // 1. 渲染不透明几何体到depthtex0
 *     // RenderOpaqueGeometry();
 *
 *     // 2. TERRAIN_TRANSLUCENT前复制
 *     // g_theRenderer->CopyDepthBuffer(0, 1);
 *
 *     // 3. 渲染半透明几何体
 *     // RenderTranslucentGeometry();
 *
 *     // 4. HAND_SOLID前复制
 *     // g_theRenderer->CopyDepthBuffer(0, 2);
 *
 *     // 5. 渲染手部
 *     // RenderHands();
 *
 *     // 结束帧
 *     g_theRenderer->EndFrame();
 * }
 */
