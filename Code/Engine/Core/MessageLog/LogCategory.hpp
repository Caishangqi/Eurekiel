#pragma once
#include <string>
#include "../Rgba8.hpp"

namespace enigma::core
{
    // Category元数据结构
    struct CategoryMetadata
    {
        std::string name;               // Category名称（内部ID）
        std::string displayName;        // 显示名称（可选，为空则使用name）
        Rgba8 defaultColor;             // 默认颜色
        bool visible = true;            // 是否可见

        // 默认构造函数
        CategoryMetadata() = default;

        // 完整构造函数
        CategoryMetadata(const std::string& name,
                         const Rgba8& color = Rgba8::WHITE,
                         const std::string& displayName = "")
            : name(name)
            , displayName(displayName.empty() ? name : displayName)
            , defaultColor(color)
            , visible(true)
        {
        }
    };
}
