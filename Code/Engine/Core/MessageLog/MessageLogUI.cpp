#include "MessageLogUI.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"
#include <ThirdParty/imgui/imgui.h>
#include <algorithm>
#include <ctime>
#include <chrono>
#include <fstream>

namespace enigma::core
{
    // ================================================================================================
    // 构造函数和析构函数
    // ================================================================================================

    MessageLogUI::MessageLogUI()
    {
        // 初始化级别过滤按钮状态（默认全部可见）
        m_showVerbose = true;
        m_showInfo = true;
        m_showWarning = true;
        m_showError = true;
        m_showFatal = true;

        // 添加初始消息
        AddMessage("System", "Info", "MessageLog UI initialized successfully");
    }

    MessageLogUI::~MessageLogUI()
    {
    }

    // ================================================================================================
    // 主渲染函数
    // ================================================================================================

    void MessageLogUI::Render()
    {
        if (!m_config.showWindow)
            return;

        // 应用过滤器（如果需要更新）
        if (m_needsFilterUpdate)
        {
            ApplyFilter();
            m_needsFilterUpdate = false;
        }

        // 设置窗口大小
        ImGui::SetNextWindowSize(ImVec2(m_config.windowWidth, m_config.windowHeight), ImGuiCond_FirstUseEver);

        // 开始渲染窗口
        bool windowOpen = m_config.showWindow;
        if (!ImGui::Begin("MessageLog - Press ~ to toggle", &windowOpen))
        {
            ImGui::End();
            m_config.showWindow = windowOpen;
            return;
        }
        m_config.showWindow = windowOpen;

        // 主消息显示区域（占据大部分空间）
        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        float toolbarHeight = 70.0f;  // 底部工具栏高度（包含状态栏）

        ImGui::BeginChild("MessageListRegion", ImVec2(0, contentSize.y - toolbarHeight), true);
        RenderMessageList();
        ImGui::EndChild();

        // 底部工具栏
        RenderBottomToolbar();

        ImGui::End();
    }

    // ================================================================================================
    // 消息列表渲染
    // ================================================================================================

    void MessageLogUI::RenderMessageList()
    {
        // 使用表格布局
        if (ImGui::BeginTable("MessageTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
        {
            // 设置列宽
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100.0f);
            ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

            // 表头
            ImGui::TableHeadersRow();

            // 虚拟滚动渲染消息列表（性能优化）
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(m_filteredIndices.size()));

            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                {
                    size_t msgIndex = m_filteredIndices[row];
                    const DisplayMessage& msg = m_allMessages[msgIndex];

                    ImGui::TableNextRow();

                    // 检查是否选中
                    bool isSelected = IsSelected(msgIndex);

                    // 设置行背景色（选中时高亮）
                    if (isSelected)
                    {
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                            ImGui::GetColorU32(ImVec4(0.3f, 0.5f, 0.8f, 0.3f)));
                    }

                    // 渲染可选择的行
                    ImGui::PushID(row);

                    // 时间戳列
                    ImGui::TableSetColumnIndex(0);
                    if (ImGui::Selectable(msg.timestamp.c_str(), isSelected,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                    {
                        HandleSelection(msgIndex);
                    }

                    // 双击复制
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    {
                        CopyMessageToClipboard(msg);
                    }

                    // 类别列
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", msg.category.c_str());

                    // 级别列（带颜色）
                    ImGui::TableSetColumnIndex(2);
                    const float* color = GetLevelColor(msg.level);
                    ImGui::TextColored(ImVec4(color[0], color[1], color[2], color[3]),
                        "%s", msg.level.c_str());

                    // 消息列
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextColored(ImVec4(color[0], color[1], color[2], color[3]),
                        "%s", msg.message.c_str());

                    ImGui::PopID();
                }
            }

            clipper.End();

            // 检测Ctrl+C复制
            if (ImGui::IsWindowFocused() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) &&
                ImGui::IsKeyPressed(ImGuiKey_C))
            {
                CopySelectedToClipboard();
            }

            // 检测Ctrl+A全选
            if (ImGui::IsWindowFocused() && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) &&
                ImGui::IsKeyPressed(ImGuiKey_A))
            {
                SelectAll();
            }

            // 自动滚动到底部
            if (m_scrollToBottom && m_config.autoScroll)
            {
                ImGui::SetScrollHereY(1.0f);
                m_scrollToBottom = false;
            }

            ImGui::EndTable();
        }
    }

    // ================================================================================================
    // 底部工具栏渲染
    // ================================================================================================

    void MessageLogUI::RenderBottomToolbar()
    {
        ImGui::Separator();
        ImGui::BeginGroup();

        // 1. 搜索框（左侧）
        ImGui::PushItemWidth(200.0f);
        if (ImGui::InputTextWithHint("##Search", "Search...", m_searchBuffer, sizeof(m_searchBuffer)))
        {
            m_needsFilterUpdate = true;
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();

        // 2. Filters按钮（点击展开菜单）
        if (ImGui::Button("Filters"))
        {
            ImGui::OpenPopup("FiltersPopup");
        }

        // 渲染过滤器弹出菜单
        if (ImGui::BeginPopup("FiltersPopup"))
        {
            RenderFilterMenu();
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        // 3. Clear按钮
        if (ImGui::Button("Clear"))
        {
            Clear();
        }

        ImGui::SameLine();

        // 4. Export按钮
        if (ImGui::Button("Export"))
        {
            ExportToFile("MessageLog_Export.txt");
        }

        ImGui::EndGroup();

        // 状态栏
        RenderStatusBar();
    }

    // ================================================================================================
    // 过滤器菜单渲染
    // ================================================================================================

    void MessageLogUI::RenderFilterMenu()
    {
        // Verbosity子菜单
        if (ImGui::BeginMenu("Verbosity"))
        {
            if (ImGui::MenuItem("Verbose", nullptr, &m_showVerbose))
            {
                m_needsFilterUpdate = true;
            }
            if (ImGui::MenuItem("Info", nullptr, &m_showInfo))
            {
                m_needsFilterUpdate = true;
            }
            if (ImGui::MenuItem("Warning", nullptr, &m_showWarning))
            {
                m_needsFilterUpdate = true;
            }
            if (ImGui::MenuItem("Error", nullptr, &m_showError))
            {
                m_needsFilterUpdate = true;
            }
            if (ImGui::MenuItem("Fatal", nullptr, &m_showFatal))
            {
                m_needsFilterUpdate = true;
            }

            ImGui::EndMenu();
        }

        // Categories子菜单
        if (ImGui::BeginMenu("Categories"))
        {
            // "All"选项（使用Radio样式）
            bool isAllSelected = (m_selectedCategory == "All" || m_selectedCategory.empty());
            if (ImGui::MenuItem("All", nullptr, isAllSelected))
            {
                m_selectedCategory = "All";
                m_needsFilterUpdate = true;
            }

            ImGui::Separator();

            // 更新类别消息数量
            UpdateMessageCounts();

            // 类别列表
            for (const auto& category : m_allCategories)
            {
                std::string label = category + " (" +
                    std::to_string(m_categoryMessageCount[category]) + ")";

                bool isSelected = (m_selectedCategory == category);

                if (ImGui::MenuItem(label.c_str(), nullptr, isSelected))
                {
                    m_selectedCategory = category;
                    m_needsFilterUpdate = true;
                }
            }

            ImGui::EndMenu();
        }
    }

    // ================================================================================================
    // 状态栏渲染
    // ================================================================================================

    void MessageLogUI::RenderStatusBar()
    {
        ImGui::Text("Total: %zu | Filtered: %zu | Selected: %zu",
            m_allMessages.size(),
            m_filteredIndices.size(),
            m_selectedIndices.size());

        ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
        ImGui::Checkbox("Auto-scroll", &m_config.autoScroll);
    }

    // ================================================================================================
    // 消息管理
    // ================================================================================================

    void MessageLogUI::AddMessage(const std::string& category, const std::string& level, const std::string& message)
    {
        // 获取当前时间戳
        std::string timestamp = GetCurrentTimestamp();

        // 创建消息
        DisplayMessage msg(timestamp, category, level, message);

        // 添加到消息列表
        m_allMessages.push_back(msg);

        // 限制消息数量
        if (m_allMessages.size() > m_config.maxMessages)
        {
            m_allMessages.pop_front();
        }

        // 记录类别
        m_allCategories.insert(category);

        // 标记需要更新过滤
        m_needsFilterUpdate = true;
        m_scrollToBottom = true;
    }

    void MessageLogUI::Clear()
    {
        m_allMessages.clear();
        m_filteredIndices.clear();
        m_selectedIndices.clear();
        m_allCategories.clear();
        m_categoryMessageCount.clear();
        m_needsFilterUpdate = true;

        // 添加清空提示消息
        AddMessage("System", "Info", "Message log cleared");
    }

    void MessageLogUI::ExportToFile(const std::string& filepath)
    {
        std::ofstream outFile(filepath);
        if (!outFile.is_open())
        {
            AddMessage("System", "Error", "Failed to export to file: " + filepath);
            return;
        }

        // 写入表头
        outFile << "MessageLog Export - " << GetCurrentTimestamp() << "\n";
        outFile << "Total Messages: " << m_allMessages.size() << "\n";
        outFile << "========================================\n\n";

        // 写入消息
        for (const auto& msg : m_allMessages)
        {
            outFile << "[" << msg.timestamp << "] ";
            outFile << "[" << msg.category << "] ";
            outFile << "[" << msg.level << "] ";
            outFile << msg.message << "\n";
        }

        outFile.close();
        AddMessage("System", "Info", "Exported " + std::to_string(m_allMessages.size()) + " messages to " + filepath);
    }

    // ================================================================================================
    // 过滤逻辑
    // ================================================================================================

    void MessageLogUI::ApplyFilter()
    {
        m_filteredIndices.clear();

        for (size_t i = 0; i < m_allMessages.size(); ++i)
        {
            if (PassesFilter(m_allMessages[i]))
            {
                m_filteredIndices.push_back(i);
            }
        }
    }

    bool MessageLogUI::PassesFilter(const DisplayMessage& msg) const
    {
        // 级别过滤（使用toggle按钮状态）
        if (msg.level == "Verbose" && !m_showVerbose) return false;
        if (msg.level == "Info" && !m_showInfo) return false;
        if (msg.level == "Warning" && !m_showWarning) return false;
        if (msg.level == "Error" && !m_showError) return false;
        if (msg.level == "Fatal" && !m_showFatal) return false;

        // 类别过滤
        if (!m_selectedCategory.empty() && m_selectedCategory != "All")
        {
            if (msg.category != m_selectedCategory)
                return false;
        }

        // 搜索框过滤
        if (m_searchBuffer[0] != '\0')
        {
            std::string lowerSearch = ToLowerCase(m_searchBuffer);
            if (msg.searchableText.find(lowerSearch) == std::string::npos)
                return false;
        }

        return true;
    }

    // ================================================================================================
    // 选取功能
    // ================================================================================================

    void MessageLogUI::HandleSelection(size_t index)
    {
        bool isCtrlDown = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
        bool isShiftDown = ImGui::IsKeyDown(ImGuiKey_LeftShift);

        if (isShiftDown && m_lastClickedIndex != SIZE_MAX)
        {
            // Shift选择：选中范围
            size_t start = std::min(m_lastClickedIndex, index);
            size_t end = std::max(m_lastClickedIndex, index);

            for (size_t i = 0; i < m_filteredIndices.size(); i++)
            {
                size_t msgIndex = m_filteredIndices[i];
                if (msgIndex >= start && msgIndex <= end)
                {
                    m_selectedIndices.insert(msgIndex);
                }
            }
        }
        else if (isCtrlDown)
        {
            // Ctrl选择：切换选中状态
            if (m_selectedIndices.count(index))
            {
                m_selectedIndices.erase(index);
            }
            else
            {
                m_selectedIndices.insert(index);
            }
        }
        else
        {
            // 普通点击：单选
            m_selectedIndices.clear();
            m_selectedIndices.insert(index);
        }

        m_lastClickedIndex = index;
    }

    void MessageLogUI::CopySelectedToClipboard()
    {
        if (m_selectedIndices.empty())
            return;

        std::string clipboardText;

        // 按显示顺序复制选中的消息
        for (size_t idx : m_filteredIndices)
        {
            if (m_selectedIndices.count(idx))
            {
                const DisplayMessage& msg = m_allMessages[idx];
                clipboardText += "[" + msg.timestamp + "] [" + msg.level + "] " +
                    msg.category + ": " + msg.message + "\n";
            }
        }

        // 复制到剪贴板
        ImGui::SetClipboardText(clipboardText.c_str());

        DebuggerPrintf("[MessageLog] Copied %zu messages to clipboard\n",
            m_selectedIndices.size());
    }

    void MessageLogUI::CopyMessageToClipboard(const DisplayMessage& msg)
    {
        std::string text = "[" + msg.timestamp + "] [" + msg.level + "] " +
            msg.category + ": " + msg.message;

        ImGui::SetClipboardText(text.c_str());
    }

    bool MessageLogUI::IsSelected(size_t index) const
    {
        return m_selectedIndices.count(index) > 0;
    }

    void MessageLogUI::SelectAll()
    {
        m_selectedIndices.clear();
        for (size_t idx : m_filteredIndices)
        {
            m_selectedIndices.insert(idx);
        }
    }

    // ================================================================================================
    // 类别和级别过滤
    // ================================================================================================

    void MessageLogUI::UpdateMessageCounts()
    {
        // 重置计数
        m_categoryMessageCount.clear();

        // 统计消息数量
        for (const auto& msg : m_allMessages)
        {
            m_categoryMessageCount[msg.category]++;
        }
    }

    // ================================================================================================
    // 辅助函数
    // ================================================================================================

    const float* MessageLogUI::GetLevelColor(const std::string& level) const
    {
        static const float ColorVerbose[4] = {0.6f, 0.6f, 0.6f, 1.0f};  // 灰色
        static const float ColorInfo[4] = {1.0f, 1.0f, 1.0f, 1.0f};     // 白色
        static const float ColorWarning[4] = {1.0f, 0.8f, 0.0f, 1.0f};  // 黄色
        static const float ColorError[4] = {1.0f, 0.3f, 0.3f, 1.0f};    // 红色
        static const float ColorFatal[4] = {0.8f, 0.0f, 0.0f, 1.0f};    // 深红色

        if (level == "Verbose") return ColorVerbose;
        if (level == "Info") return ColorInfo;
        if (level == "Warning") return ColorWarning;
        if (level == "Error") return ColorError;
        if (level == "Fatal") return ColorFatal;

        return ColorInfo;  // 默认白色
    }

    std::string MessageLogUI::GetCurrentTimestamp() const
    {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        struct tm timeinfo;
        localtime_s(&timeinfo, &time);

        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        return std::string(buffer);
    }

    std::string MessageLogUI::ToLowerCase(const std::string& str) const
    {
        std::string result = str;
        for (char& c : result)
        {
            c = static_cast<char>(tolower(c));
        }
        return result;
    }

    void MessageLogUI::ToggleWindow()
    {
        m_config.showWindow = !m_config.showWindow;
    }
}
