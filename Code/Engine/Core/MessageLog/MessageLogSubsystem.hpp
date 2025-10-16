#pragma once
#include "../SubsystemManager.hpp"
#include "LogMessage.hpp"
#include "LogCategory.hpp"
#include "LogFilter.hpp"
#include "MessageBuffer.hpp"
#include "MessageLogUI.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <memory>

namespace enigma::core
{
    // 消息日志子系统
    // 负责管理运行时的所有日志消息，提供Category管理和基础过滤功能
    class MessageLogSubsystem : public EngineSubsystem
    {
    public:
        MessageLogSubsystem();
        ~MessageLogSubsystem() override;

        // EngineSubsystem接口实现
        void Initialize() override;
        void Startup() override;
        void Shutdown() override;
        void Update(float deltaTime) override;

        // 子系统信息
        DECLARE_SUBSYSTEM(MessageLogSubsystem, "MessageLogSubsystem", 300);

        // 需要每帧更新（检测~键）
        bool RequiresGameLoop() const override { return true; }
        bool RequiresInitialize() const override { return true; }

        //---------------------------------------------------------------------
        // Category管理
        //---------------------------------------------------------------------

        // 注册Category
        void RegisterCategory(
            const std::string& name,
            const std::string& displayName = "",
            const Rgba8& color = Rgba8::WHITE
        );

        // 检查Category是否已注册
        bool IsCategoryRegistered(const std::string& name) const;

        // 获取Category元数据
        CategoryMetadata GetCategoryMetadata(const std::string& name) const;

        // 获取所有Category
        std::vector<CategoryMetadata> GetAllCategories() const;

        // 设置Category可见性
        void SetCategoryVisible(const std::string& name, bool visible);

        // 检查Category是否可见
        bool IsCategoryVisible(const std::string& name) const;

        //---------------------------------------------------------------------
        // 消息管理
        //---------------------------------------------------------------------

        // 添加消息（完整版本）
        void AddMessage(const MessageLogEntry& message);

        // 添加消息（简化版本）
        void AddMessage(
            LogLevel level,
            const std::string& category,
            const std::string& message
        );

        // 获取消息数量
        size_t GetMessageCount() const;

        // 获取消息（按索引）
        const MessageLogEntry& RetrieveMessage(size_t index) const;

        // 清空所有消息
        void ClearMessages();

        //---------------------------------------------------------------------
        // 基础过滤（Phase 3将扩展为高级过滤）
        //---------------------------------------------------------------------

        // 设置LogLevel过滤器
        void SetLevelFilter(const std::set<LogLevel>& levels);

        // 设置Category过滤器
        void SetCategoryFilter(const std::set<std::string>& categories);

        // 获取过滤后的消息列表
        std::vector<const MessageLogEntry*> GetFilteredMessages() const;

        // 获取过滤后的消息数量
        size_t GetFilteredMessageCount() const;

        //---------------------------------------------------------------------
        // 配置
        //---------------------------------------------------------------------

        // 设置最大消息数量
        void SetMaxMessageCount(size_t maxCount);

        // 获取最大消息数量
        size_t GetMaxMessageCount() const;

        //---------------------------------------------------------------------
        // UI访问
        //---------------------------------------------------------------------

        // 获取UI实例（用于配置）
        MessageLogUI* GetUI() { return m_ui.get(); }

    private:
        // 判断消息是否通过过滤器
        bool PassesFilter(const MessageLogEntry& message) const;

        // 获取Category的颜色（如果Category已注册则使用其默认颜色）
        Rgba8 GetCategoryColor(const std::string& category) const;

        // 消息缓冲区
        MessageBuffer m_messageBuffer;

        // Category注册表
        std::unordered_map<std::string, CategoryMetadata> m_categories;
        mutable std::mutex m_categoryMutex;

        // 基础过滤器
        BasicLogFilter m_filter;

        // 当前帧号（用于给消息打标记）
        uint32_t m_currentFrameNumber = 0;

        // UI实例
        std::unique_ptr<MessageLogUI> m_ui;
    };
}
