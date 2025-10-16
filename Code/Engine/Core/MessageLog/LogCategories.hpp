#pragma once

// 定义宏：自动生成Category字符串常量
#define LOG_CATEGORY_DEFINE(CategoryName, CategoryString) \
    namespace LogCategories { \
        constexpr const char* CategoryName = CategoryString; \
    }

namespace enigma::core
{
    // 预定义核心Category常量（使用混合方式 - 方案C）
    // 这些常量可以在编译期使用，避免字符串查找开销
    // 同时也支持直接使用字符串进行动态注册

    LOG_CATEGORY_DEFINE(Engine, "LogEngine");
    LOG_CATEGORY_DEFINE(Game, "LogGame");
    LOG_CATEGORY_DEFINE(Renderer, "LogRenderer");
    LOG_CATEGORY_DEFINE(Audio, "LogAudio");
    LOG_CATEGORY_DEFINE(Physics, "LogPhysics");
    LOG_CATEGORY_DEFINE(Network, "LogNetwork");
    LOG_CATEGORY_DEFINE(UI, "LogUI");
    LOG_CATEGORY_DEFINE(Input, "LogInput");
    LOG_CATEGORY_DEFINE(Resource, "LogResource");
    LOG_CATEGORY_DEFINE(AI, "LogAI");
}
