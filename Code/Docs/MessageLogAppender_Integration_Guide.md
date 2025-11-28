# MessageLogAppender 集成指南

## 概述

`MessageLogAppender` 是 Logger 到 MessageLog 的桥接，自动将 LoggerSubsystem 的输出转发到 MessageLogSubsystem。

## 文件位置

- **头文件**: `Engine/Code/Engine/Core/MessageLog/MessageLogAppender.hpp`
- **实现**: `Engine/Code/Engine/Core/MessageLog/MessageLogAppender.cpp`

## 功能特性

- **自动转发**: 将 Logger 的日志消息自动转发到 MessageLog
- **类型转换**: 处理 Logger::LogMessage 到 MessageLog::LogMessage 的转换
- **线程安全**: 使用 mutex 保护 Write 操作
- **解耦设计**: Logger 和 MessageLog 保持独立，通过 Appender 桥接

## 使用方法

### 在 Engine 初始化时注册 Appender

在 `Engine::Startup()` 或类似的初始化代码中添加：

```cpp
void Engine::Startup()
{
    // ... 其他初始化代码 ...

    // 注册 MessageLogAppender
    RegisterMessageLogAppender();

    // ... 其他初始化代码 ...
}

void Engine::RegisterMessageLogAppender()
{
    auto* logger = GetSubsystem<LoggerSubsystem>();
    auto* msgLog = GetSubsystem<MessageLogSubsystem>();

    if (logger && msgLog)
    {
        // 创建 MessageLogAppender
        auto appender = std::make_unique<MessageLogAppender>(msgLog);

        // 将 Appender 添加到 Logger
        logger->AddAppender(std::move(appender));

        LOG_INFO("Engine", "MessageLogAppender registered successfully");
    }
    else
    {
        LOG_WARN("Engine", "Failed to register MessageLogAppender: Logger or MessageLog not available");
    }
}
```

### 或者使用全局函数（如果有）

```cpp
void RegisterMessageLogAppender()
{
    auto* logger = GEngine->GetLogger();  // 或 GetSubsystem<LoggerSubsystem>()
    auto* msgLog = GEngine->GetMessageLog();  // 或 GetSubsystem<MessageLogSubsystem>()

    if (logger && msgLog)
    {
        auto appender = std::make_unique<MessageLogAppender>(msgLog);
        logger->AddAppender(std::move(appender));
    }
}
```

## 工作原理

### 1. Logger 输出消息

```cpp
LOG_INFO("Renderer", "Texture loaded: texture.png");
```

### 2. MessageLogAppender 接收消息

Logger 将消息传递给所有已注册的 Appenders，包括 MessageLogAppender。

### 3. 类型转换

MessageLogAppender 将 `Logger::LogMessage` 转换为 `MessageLog::LogMessage`：

```cpp
// Logger::LogMessage 包含:
// - LogLevel level
// - std::string category
// - std::string message
// - std::chrono::system_clock::time_point timestamp
// - std::thread::id threadId
// - int frameNumber

// MessageLog::LogMessage 包含:
// - uint64_t id (由 MessageLogSubsystem 自动生成)
// - TimeStamp timestamp (std::chrono::system_clock::time_point)
// - uint32_t frameNumber
// - LogLevel level
// - std::string category
// - Rgba8 color (基于 LogLevel)
// - std::string message
```

### 4. 转发到 MessageLog

```cpp
m_messageLog->AddMessage(
    loggerMessage.level,
    loggerMessage.category,
    loggerMessage.message
);
```

## 配置选项

### 启用/禁用 Appender

```cpp
auto* logger = GetSubsystem<LoggerSubsystem>();
if (logger)
{
    // 获取 MessageLogAppender (需要额外的接口支持)
    // logger->GetAppender<MessageLogAppender>()->SetEnabled(false);

    // 或在创建时禁用
    auto appender = std::make_unique<MessageLogAppender>(msgLog);
    appender->SetEnabled(false);
    logger->AddAppender(std::move(appender));
}
```

## 性能考虑

- **轻量级**: MessageLogAppender 只做简单的类型转换和转发，几乎没有性能开销
- **线程安全**: 使用 mutex 保护，多线程环境下安全
- **无缓冲**: 消息立即转发，不引入额外延迟

## 注意事项

### 1. 初始化顺序

确保 LoggerSubsystem 和 MessageLogSubsystem 都已初始化后再注册 Appender：

- LoggerSubsystem 优先级: 100 (最高)
- MessageLogSubsystem 优先级: 300

因此在 Engine::Startup() 中注册 Appender 是安全的。

### 2. 时间戳和帧号

当前实现中，MessageLogSubsystem 会使用自己的时间戳和帧号，而不是 Logger 的。

如果需要保留 Logger 的时间戳和帧号，可以修改为使用完整版本的 AddMessage：

```cpp
// 在 MessageLogAppender.cpp 的 Write 方法中
enigma::core::LogMessage msgLogMessage;
msgLogMessage.level = loggerMessage.level;
msgLogMessage.category = loggerMessage.category;
msgLogMessage.message = loggerMessage.message;
msgLogMessage.timestamp = loggerMessage.timestamp;
msgLogMessage.frameNumber = static_cast<uint32_t>(loggerMessage.frameNumber);
msgLogMessage.color = GetColorForLogLevel(loggerMessage.level);
// msgLogMessage.id 由 MessageLogSubsystem 设置
m_messageLog->AddMessage(msgLogMessage);
```

### 3. Category 颜色

当前实现使用 LogLevel 的默认颜色。

如果 MessageLog 已注册了 Category 的自定义颜色，可以优先使用 Category 颜色：

```cpp
Rgba8 color;
if (m_messageLog->IsCategoryRegistered(loggerMessage.category))
{
    auto metadata = m_messageLog->GetCategoryMetadata(loggerMessage.category);
    color = metadata.color;
}
else
{
    color = GetColorForLogLevel(loggerMessage.level);
}
```

## 调试与验证

### 验证 Appender 是否工作

1. 在 Engine 启动后发送一条日志：

```cpp
LOG_INFO("Test", "Testing MessageLogAppender");
```

2. 检查 MessageLog 是否收到消息：

```cpp
auto* msgLog = GetSubsystem<MessageLogSubsystem>();
if (msgLog)
{
    size_t count = msgLog->GetMessageCount();
    // 应该能看到至少一条消息
}
```

3. 在 DevConsole 或其他 UI 中查看 MessageLog 的内容

## Phase 2 扩展

在 Phase 2 中，可以扩展 MessageLogAppender 以支持：

- **Category 颜色优先**: 优先使用 MessageLog 中注册的 Category 颜色
- **时间戳保留**: 选项以保留 Logger 的原始时间戳
- **过滤器**: 只转发特定 Category 或 LogLevel 的消息
- **格式化**: 在转发前对消息进行格式化

## 相关文档

- MessageLog 系统规划: `.claude/plan/messagelog-system.md`
- Logger 子系统文档: `Engine/Code/Engine/Core/Logger/Logger.hpp`
- MessageLog 子系统文档: `Engine/Code/Engine/Core/MessageLog/MessageLogSubsystem.hpp`
