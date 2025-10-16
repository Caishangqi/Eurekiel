#pragma once
#include "Engine/Core/Rgba8.hpp"
#include <string>
#include <vector>
#include <deque>
#include <unordered_set>
#include <unordered_map>

namespace enigma::core
{
    // 日志消息结构（简化版，用于显示）
    struct DisplayMessage
    {
        std::string timestamp;      // 时间戳 HH:MM:SS
        std::string category;       // 类别（Game, Render, Audio等）
        std::string level;          // 级别（Verbose, Info, Warning, Error, Fatal）
        std::string message;        // 消息内容

        // 用于过滤的小写文本
        std::string searchableText; // 全部小写的可搜索文本（category + message）

        DisplayMessage() = default;
        DisplayMessage(const std::string& ts, const std::string& cat,
                       const std::string& lvl, const std::string& msg)
            : timestamp(ts), category(cat), level(lvl), message(msg)
        {
            // 构造可搜索文本（小写）
            searchableText = category + " " + message;
            for (char& c : searchableText)
            {
                c = static_cast<char>(tolower(c));
            }
        }
    };

    // MessageLog UI配置
    struct MessageLogUIConfig
    {
        bool showWindow = false;         // 窗口是否显示
        int toggleKey = 0xC0;            // 开关按键（默认~键，VK_OEM_3）
        size_t maxMessages = 10000;      // 最大消息数量
        bool autoScroll = true;          // 自动滚动到底部
        float windowWidth = 1000.0f;     // 窗口宽度
        float windowHeight = 600.0f;     // 窗口高度
    };

    // MessageLog UI类（增强版 - 支持选取、复制、类别面板）
    class MessageLogUI
    {
    public:
        MessageLogUI();
        ~MessageLogUI();

        // 渲染UI
        void Render();

        // 添加消息
        void AddMessage(const std::string& category, const std::string& level, const std::string& message);

        // 开关窗口
        void ToggleWindow();
        bool IsWindowOpen() const { return m_config.showWindow; }
        void SetWindowOpen(bool open) { m_config.showWindow = open; }

        // 配置
        MessageLogUIConfig& GetConfig() { return m_config; }
        const MessageLogUIConfig& GetConfig() const { return m_config; }

        // 清空消息
        void Clear();

        // 导出消息到文件
        void ExportToFile(const std::string& filepath);

    private:
        // 渲染各部分
        void RenderMessageList();
        void RenderBottomToolbar();
        void RenderFilterMenu();
        void RenderStatusBar();

        // 过滤逻辑
        void ApplyFilter();
        bool PassesFilter(const DisplayMessage& msg) const;

        // 选取功能
        void HandleSelection(size_t index);
        void CopySelectedToClipboard();
        void CopyMessageToClipboard(const DisplayMessage& msg);
        bool IsSelected(size_t index) const;
        void SelectAll();

        // 类别和级别过滤
        void UpdateMessageCounts();

        // 辅助函数
        const float* GetLevelColor(const std::string& level) const;
        std::string GetCurrentTimestamp() const;
        std::string ToLowerCase(const std::string& str) const;

        // 配置
        MessageLogUIConfig m_config;

        // 消息存储
        std::deque<DisplayMessage> m_allMessages;        // 所有消息
        std::vector<size_t> m_filteredIndices;           // 过滤后的消息索引

        // 过滤器状态
        char m_searchBuffer[256] = {0};                  // 搜索框内容
        std::string m_selectedCategory = "All";          // 选中的类别
        std::unordered_set<std::string> m_allCategories; // 所有出现过的类别

        // 选取状态
        std::unordered_set<size_t> m_selectedIndices;    // 选中的消息索引
        size_t m_lastClickedIndex = SIZE_MAX;            // 最后点击的索引（用于Shift选取）

        // 底部工具栏状态（级别过滤按钮）
        bool m_showVerbose = true;                       // 显示Verbose消息
        bool m_showInfo = true;                          // 显示Info消息
        bool m_showWarning = true;                       // 显示Warning消息
        bool m_showError = true;                         // 显示Error消息
        bool m_showFatal = true;                         // 显示Fatal消息

        // 类别过滤状态
        std::unordered_map<std::string, int> m_categoryMessageCount; // 类别消息数量

        // UI状态
        bool m_needsFilterUpdate = true;                 // 需要更新过滤
        bool m_scrollToBottom = false;                   // 需要滚动到底部
    };
}
