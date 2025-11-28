# MessageLogAppender 测试计划

## 测试目标

验证 MessageLogAppender 能够正确地将 Logger 的输出转发到 MessageLog。

## 前置条件

1. Engine 已初始化
2. LoggerSubsystem 已启动
3. MessageLogSubsystem 已启动
4. MessageLogAppender 已注册到 LoggerSubsystem

## 测试用例

### 测试 1: 基本消息转发

**描述**: 验证简单的日志消息能够被正确转发。

**步骤**:
1. 使用 Logger 发送一条 INFO 级别的消息
2. 检查 MessageLog 是否收到该消息

**代码**:
```cpp
void Test_BasicMessageForwarding()
{
    // 发送消息
    LOG_INFO("Test", "Basic forwarding test");

    // 验证 MessageLog 收到消息
    auto* msgLog = GEngine->GetSubsystem<MessageLogSubsystem>();
    REQUIRE(msgLog != nullptr);

    size_t count = msgLog->GetMessageCount();
    REQUIRE(count > 0);

    // 检查最后一条消息
    const auto& lastMsg = msgLog->GetMessage(count - 1);
    REQUIRE(lastMsg.category == "Test");
    REQUIRE(lastMsg.message == "Basic forwarding test");
    REQUIRE(lastMsg.level == LogLevel::INFO);
}
```

**预期结果**: MessageLog 包含发送的消息。

---

### 测试 2: 不同 LogLevel 的消息

**描述**: 验证所有 LogLevel 的消息都能被正确转发。

**步骤**:
1. 发送 TRACE, DEBUG, INFO, WARNING, ERROR, FATAL 级别的消息
2. 验证 MessageLog 收到所有消息

**代码**:
```cpp
void Test_AllLogLevels()
{
    auto* msgLog = GEngine->GetSubsystem<MessageLogSubsystem>();
    size_t initialCount = msgLog->GetMessageCount();

    // 发送不同级别的消息
    LOG_TRACE("Test", "Trace message");
    LOG_DEBUG("Test", "Debug message");
    LOG_INFO("Test", "Info message");
    LOG_WARN("Test", "Warning message");
    LOG_ERROR("Test", "Error message");
    LOG_FATAL("Test", "Fatal message");

    // 验证消息数量
    size_t finalCount = msgLog->GetMessageCount();
    REQUIRE(finalCount == initialCount + 6);

    // 验证每条消息的级别
    std::vector<LogLevel> expectedLevels = {
        LogLevel::TRACE,
        LogLevel::DEBUG,
        LogLevel::INFO,
        LogLevel::WARNING,
        LogLevel::ERROR_,
        LogLevel::FATAL
    };

    for (size_t i = 0; i < 6; i++)
    {
        const auto& msg = msgLog->GetMessage(initialCount + i);
        REQUIRE(msg.level == expectedLevels[i]);
    }
}
```

**预期结果**: MessageLog 包含所有 6 条不同级别的消息。

---

### 测试 3: 不同 Category 的消息

**描述**: 验证不同 Category 的消息能够被正确转发。

**步骤**:
1. 发送不同 Category 的消息（Engine, Renderer, Audio, Game）
2. 验证 MessageLog 收到所有消息并保留正确的 Category

**代码**:
```cpp
void Test_DifferentCategories()
{
    auto* msgLog = GEngine->GetSubsystem<MessageLogSubsystem>();
    size_t initialCount = msgLog->GetMessageCount();

    // 发送不同 Category 的消息
    LOG_INFO("Engine", "Engine message");
    LOG_INFO("Renderer", "Renderer message");
    LOG_INFO("Audio", "Audio message");
    LOG_INFO("Game", "Game message");

    // 验证消息数量
    size_t finalCount = msgLog->GetMessageCount();
    REQUIRE(finalCount == initialCount + 4);

    // 验证每条消息的 Category
    std::vector<std::string> expectedCategories = {
        "Engine", "Renderer", "Audio", "Game"
    };

    for (size_t i = 0; i < 4; i++)
    {
        const auto& msg = msgLog->GetMessage(initialCount + i);
        REQUIRE(msg.category == expectedCategories[i]);
    }
}
```

**预期结果**: MessageLog 包含所有 4 条不同 Category 的消息。

---

### 测试 4: 线程安全性

**描述**: 验证 MessageLogAppender 在多线程环境下的安全性。

**步骤**:
1. 创建多个线程
2. 每个线程发送多条日志消息
3. 验证所有消息都被正确转发且没有崩溃

**代码**:
```cpp
void Test_ThreadSafety()
{
    auto* msgLog = GEngine->GetSubsystem<MessageLogSubsystem>();
    size_t initialCount = msgLog->GetMessageCount();

    const int numThreads = 10;
    const int messagesPerThread = 100;

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    // 创建多个线程发送消息
    for (int i = 0; i < numThreads; i++)
    {
        threads.emplace_back([i, messagesPerThread]()
        {
            for (int j = 0; j < messagesPerThread; j++)
            {
                LOG_INFO("Thread" + std::to_string(i),
                         "Message " + std::to_string(j));
            }
        });
    }

    // 等待所有线程完成
    for (auto& thread : threads)
    {
        thread.join();
    }

    // 验证消息数量
    size_t finalCount = msgLog->GetMessageCount();
    REQUIRE(finalCount == initialCount + (numThreads * messagesPerThread));
}
```

**预期结果**: 所有 1000 条消息都被正确转发，程序没有崩溃。

---

### 测试 5: Appender 启用/禁用

**描述**: 验证禁用 Appender 后消息不再转发。

**步骤**:
1. 发送一条消息，验证转发成功
2. 禁用 MessageLogAppender
3. 发送另一条消息，验证没有被转发
4. 重新启用 MessageLogAppender
5. 发送第三条消息，验证转发恢复

**代码**:
```cpp
void Test_EnableDisable()
{
    auto* msgLog = GEngine->GetSubsystem<MessageLogSubsystem>();
    auto* logger = GEngine->GetSubsystem<LoggerSubsystem>();

    // 获取 MessageLogAppender 的引用（需要额外的接口支持）
    // 这里假设有一个方法可以获取 Appender
    // auto* appender = logger->GetAppender<MessageLogAppender>();

    size_t count1 = msgLog->GetMessageCount();

    // 1. 发送消息（启用状态）
    LOG_INFO("Test", "Message 1");
    size_t count2 = msgLog->GetMessageCount();
    REQUIRE(count2 == count1 + 1);

    // 2. 禁用 Appender
    // appender->SetEnabled(false);

    // 3. 发送消息（禁用状态）
    // LOG_INFO("Test", "Message 2 (should not forward)");
    // size_t count3 = msgLog->GetMessageCount();
    // REQUIRE(count3 == count2);  // 消息数量不变

    // 4. 重新启用 Appender
    // appender->SetEnabled(true);

    // 5. 发送消息（启用状态）
    // LOG_INFO("Test", "Message 3");
    // size_t count4 = msgLog->GetMessageCount();
    // REQUIRE(count4 == count3 + 1);
}
```

**注意**: 此测试需要 LoggerSubsystem 提供获取特定 Appender 的接口。

**预期结果**: 禁用后消息不转发，启用后恢复转发。

---

### 测试 6: 空指针安全性

**描述**: 验证 MessageLogAppender 在 MessageLogSubsystem 为 nullptr 时不会崩溃。

**步骤**:
1. 创建一个 MessageLogAppender，传入 nullptr
2. 尝试发送消息
3. 验证程序不崩溃

**代码**:
```cpp
void Test_NullPointerSafety()
{
    // 创建一个带空指针的 Appender
    auto appender = std::make_unique<MessageLogAppender>(nullptr);

    // 模拟 Logger 调用
    enigma::core::LogMessage msg(LogLevel::INFO, "Test", "Test message");

    // 这不应该崩溃
    REQUIRE_NOTHROW(appender->Write(msg));
}
```

**预期结果**: 程序不崩溃，静默失败。

---

## 集成测试

### 在实际 Engine 环境中测试

**步骤**:
1. 在 Engine 初始化时注册 MessageLogAppender
2. 在游戏运行时发送各种日志消息
3. 打开 DevConsole 或 MessageLog UI
4. 验证所有消息都能在 UI 中看到

**验证点**:
- 消息内容正确
- Category 正确
- LogLevel 正确
- 颜色显示正确（基于 LogLevel）
- 时间戳合理
- 帧号合理（如果显示）

---

## 性能测试

### 测试高频率日志

**描述**: 验证大量日志消息不会导致性能问题。

**步骤**:
1. 在一帧内发送 10000 条日志消息
2. 测量帧时间
3. 验证性能影响可接受

**代码**:
```cpp
void Test_HighFrequencyLogging()
{
    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; i++)
    {
        LOG_INFO("Performance", "Message " + std::to_string(i));
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // 验证时间在合理范围内（例如 < 100ms）
    REQUIRE(duration.count() < 100);
}
```

**预期结果**: 10000 条消息的处理时间 < 100ms。

---

## 回归测试

在修改 MessageLogAppender 后，重新运行所有测试用例以确保没有引入 bug。

---

## 测试工具

推荐使用以下测试框架：
- Google Test (gtest)
- Catch2
- 自定义测试框架

---

## 测试报告模板

```
测试日期: YYYY-MM-DD
测试人员:
Engine 版本:

| 测试用例 | 状态 | 备注 |
|---------|------|------|
| 基本消息转发 | PASS/FAIL | |
| 不同 LogLevel | PASS/FAIL | |
| 不同 Category | PASS/FAIL | |
| 线程安全性 | PASS/FAIL | |
| 启用/禁用 | PASS/FAIL | |
| 空指针安全 | PASS/FAIL | |
| 集成测试 | PASS/FAIL | |
| 性能测试 | PASS/FAIL | |
```

---

## 已知问题与限制

1. **时间戳不保留**: 当前实现不保留 Logger 的原始时间戳，使用 MessageLog 的时间戳
2. **帧号不保留**: 当前实现不保留 Logger 的帧号，使用 MessageLog 的帧号
3. **Category 颜色**: 当前使用 LogLevel 的颜色，不使用 MessageLog 注册的 Category 颜色

这些限制将在 Phase 2 中解决。
