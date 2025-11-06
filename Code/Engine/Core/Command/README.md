# Command Subsystem

独立的命令处理子系统,提供命令注册、执行、历史管理和补全功能。

## 文件结构

- `CommandTypes.hpp` - 命令相关类型定义
- `CommandParser.hpp/cpp` - 命令解析器
- `CommandHistory.hpp` - 历史管理(header-only)
- `CommandSubsystem.hpp/cpp` - 核心子系统

## 特性

- ✅ 命令注册(支持Lambda)
- ✅ 命令解析(位置参数和命名参数)
- ✅ 历史管理(最大1000条)
- ✅ 基础补全支持
- ✅ 内置命令(help, history, clear_history)
- ✅ 线程安全

## 使用示例

```cpp
// 获取子系统
auto* cmdSys = subsystemManager->GetSubsystem<CommandSubsystem>();

// 注册命令
cmdSys->RegisterCommand("echo",
    [](const CommandArgs& args) {
        std::string text = args.GetPositional<std::string>(0, "");
        return CommandResult::Success("Echo: " + text);
    },
    "Echo a message",
    "echo <message>");

// 执行命令
auto result = cmdSys->Execute("echo Hello World");
if (result.IsSuccess()) {
    std::cout << result.message << std::endl;
}
```

## 设计原则

- **独立性**: 不依赖 DevConsole 或 MessageLog
- **可复用**: 可被多个系统使用
- **线程安全**: 支持多线程访问
- **扩展性**: 易于添加新命令和功能
