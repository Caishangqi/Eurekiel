#include "MessageLogSubsystem.hpp"
#include "MessageLogAppender.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "../Logger/LogLevel.hpp"
#include "../Logger/LoggerSubsystem.hpp"
#include "../Engine.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Game/GameCommon.hpp"

namespace enigma::core
{
    MessageLogSubsystem::MessageLogSubsystem()
        : m_messageBuffer(10000) // 默认最大10000条消息
    {
    }

    MessageLogSubsystem::~MessageLogSubsystem() = default;

    void MessageLogSubsystem::Initialize()
    {
        // 创建UI实例
        m_ui = std::make_unique<MessageLogUI>();

        // 注意: MessageLogAppender 会由 LoggerSubsystem::CreateDefaultAppenders() 自动添加
        // 无需在此手动注册,避免重复添加导致日志输出两次

        // 注册预定义的核心Category
        RegisterCategory(LogEngine.GetName(), "Engine", Rgba8(150, 200, 255, 255));
        RegisterCategory(LogCore.GetName(), "Core", Rgba8(100, 255, 100, 255));
        RegisterCategory(LogSystem.GetName(), "System", Rgba8(100, 200, 100, 255));
        RegisterCategory(LogRenderer.GetName(), "Renderer", Rgba8(255, 200, 100, 255));
        RegisterCategory(LogRender.GetName(), "Render", Rgba8(255, 180, 100, 255));
        RegisterCategory(LogGraphics.GetName(), "Graphics", Rgba8(255, 160, 100, 255));
        RegisterCategory(LogAudio.GetName(), "Audio", Rgba8(255, 150, 255, 255));
        RegisterCategory(LogInput.GetName(), "Input", Rgba8(200, 200, 255, 255));
        RegisterCategory(LogTemp.GetName(), "Temp", Rgba8(200, 200, 200, 255));

        DebuggerPrintf("[MessageLogSubsystem] Initialized with UI\n");
    }

    void MessageLogSubsystem::Startup()
    {
        // 添加启动消息
        AddMessage(LogLevel::INFO, "LogSystem", "MessageLog system started");
    }

    void MessageLogSubsystem::Update(float deltaTime)
    {
        (void)deltaTime; // Unused parameter

        // 检测~键切换窗口
        if (m_ui && g_theInput)
        {
            if (g_theInput->WasKeyJustPressed(m_ui->GetConfig().toggleKey))
            {
                m_ui->ToggleWindow();
            }
        }

        // 渲染UI
        if (m_ui)
        {
            m_ui->Render();
        }
    }

    void MessageLogSubsystem::Shutdown()
    {
        // 清空UI
        m_ui.reset();

        // 清空所有消息
        ClearMessages();

        // 清空Category注册表
        std::lock_guard<std::mutex> lock(m_categoryMutex);
        m_categories.clear();

        DebuggerPrintf("[MessageLogSubsystem] Shutdown completed\n");
    }

    //-------------------------------------------------------------------------
    // Category管理
    //-------------------------------------------------------------------------

    void MessageLogSubsystem::RegisterCategory(
        const std::string& name,
        const std::string& displayName,
        const Rgba8&       color)
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        // 如果Category已存在，更新其元数据
        if (m_categories.find(name) != m_categories.end())
        {
            m_categories[name].displayName  = displayName.empty() ? name : displayName;
            m_categories[name].defaultColor = color;
            return;
        }

        // 创建新Category
        CategoryMetadata metadata(name, color, displayName);
        m_categories[name] = metadata;
    }

    bool MessageLogSubsystem::IsCategoryRegistered(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);
        return m_categories.find(name) != m_categories.end();
    }

    CategoryMetadata MessageLogSubsystem::GetCategoryMetadata(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        auto it = m_categories.find(name);
        if (it != m_categories.end())
        {
            return it->second;
        }

        // 如果Category未注册，返回默认元数据
        return CategoryMetadata(name, Rgba8::WHITE);
    }

    std::vector<CategoryMetadata> MessageLogSubsystem::GetAllCategories() const
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        std::vector<CategoryMetadata> result;
        result.reserve(m_categories.size());

        for (const auto& pair : m_categories)
        {
            result.push_back(pair.second);
        }

        return result;
    }

    void MessageLogSubsystem::SetCategoryVisible(const std::string& name, bool visible)
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        auto it = m_categories.find(name);
        if (it != m_categories.end())
        {
            it->second.visible = visible;
        }
    }

    bool MessageLogSubsystem::IsCategoryVisible(const std::string& name) const
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        auto it = m_categories.find(name);
        if (it != m_categories.end())
        {
            return it->second.visible;
        }

        // 未注册的Category默认可见
        return true;
    }

    //-------------------------------------------------------------------------
    // 消息管理
    //-------------------------------------------------------------------------

    void MessageLogSubsystem::AddMessage(const MessageLogEntry& message)
    {
        // 创建消息副本
        MessageLogEntry newMessage = message;

        // 如果Category未注册，自动注册
        if (!IsCategoryRegistered(message.category))
        {
            RegisterCategory(message.category);
        }

        // 如果颜色未设置，使用Category的默认颜色
        if (newMessage.color.r == 255 && newMessage.color.g == 255 &&
            newMessage.color.b == 255 && newMessage.color.a == 255)
        {
            newMessage.color = GetCategoryColor(message.category);
        }

        // 设置帧号
        newMessage.frameNumber = m_currentFrameNumber;

        // 添加到缓冲区
        m_messageBuffer.AddMessage(newMessage);

        // 同时添加到UI（如果有）
        if (m_ui)
        {
            // 转换LogLevel到字符串
            std::string levelStr = LogLevelToString(message.level);
            m_ui->AddMessage(message.category, levelStr, message.message);
        }
    }

    void MessageLogSubsystem::AddMessage(
        LogLevel           level,
        const std::string& category,
        const std::string& message)
    {
        MessageLogEntry logMessage;
        logMessage.level       = level;
        logMessage.category    = category;
        logMessage.message     = message;
        logMessage.color       = GetCategoryColor(category);
        logMessage.timestamp   = std::chrono::system_clock::now();
        logMessage.frameNumber = m_currentFrameNumber;

        AddMessage(logMessage);
    }

    size_t MessageLogSubsystem::GetMessageCount() const
    {
        return m_messageBuffer.GetMessageCount();
    }

    const MessageLogEntry& MessageLogSubsystem::RetrieveMessage(size_t index) const
    {
        return m_messageBuffer.RetrieveMessage(index);
    }

    void MessageLogSubsystem::ClearMessages()
    {
        m_messageBuffer.Clear();
    }

    //-------------------------------------------------------------------------
    // 基础过滤
    //-------------------------------------------------------------------------

    void MessageLogSubsystem::SetLevelFilter(const std::set<LogLevel>& levels)
    {
        BasicFilterCriteria criteria = m_filter.GetCriteria();
        criteria.visibleLevels       = levels;
        m_filter.SetCriteria(criteria);
    }

    void MessageLogSubsystem::SetCategoryFilter(const std::set<std::string>& categories)
    {
        BasicFilterCriteria criteria = m_filter.GetCriteria();
        criteria.visibleCategories   = categories;
        m_filter.SetCriteria(criteria);
    }

    std::vector<const MessageLogEntry*> MessageLogSubsystem::GetFilteredMessages() const
    {
        std::vector<const MessageLogEntry*> result;

        // 遍历所有消息，应用过滤器
        size_t count = m_messageBuffer.GetMessageCount();
        result.reserve(count);

        for (size_t i = 0; i < count; ++i)
        {
            const MessageLogEntry& message = m_messageBuffer.RetrieveMessage(i);
            if (PassesFilter(message))
            {
                result.push_back(&message);
            }
        }

        return result;
    }

    size_t MessageLogSubsystem::GetFilteredMessageCount() const
    {
        size_t count = 0;
        size_t total = m_messageBuffer.GetMessageCount();

        for (size_t i = 0; i < total; ++i)
        {
            const MessageLogEntry& message = m_messageBuffer.RetrieveMessage(i);
            if (PassesFilter(message))
            {
                count++;
            }
        }

        return count;
    }

    //-------------------------------------------------------------------------
    // 配置
    //-------------------------------------------------------------------------

    void MessageLogSubsystem::SetMaxMessageCount(size_t maxCount)
    {
        m_messageBuffer.SetMaxSize(maxCount);
    }

    size_t MessageLogSubsystem::GetMaxMessageCount() const
    {
        return m_messageBuffer.GetMaxSize();
    }

    //-------------------------------------------------------------------------
    // 内部辅助方法
    //-------------------------------------------------------------------------

    bool MessageLogSubsystem::PassesFilter(const MessageLogEntry& message) const
    {
        // 检查Category可见性
        if (!IsCategoryVisible(message.category))
        {
            return false;
        }

        // 应用基础过滤器
        return m_filter.PassesFilter(message);
    }

    Rgba8 MessageLogSubsystem::GetCategoryColor(const std::string& category) const
    {
        std::lock_guard<std::mutex> lock(m_categoryMutex);

        auto it = m_categories.find(category);
        if (it != m_categories.end())
        {
            return it->second.defaultColor;
        }

        // 未注册的Category使用LogLevel的默认颜色
        return Rgba8::WHITE;
    }
}
