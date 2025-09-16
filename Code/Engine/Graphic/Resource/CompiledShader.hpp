#pragma once
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <functional>
#include <filesystem>
#include <chrono>
#include <d3d12.h>
#include <d3dcompiler.h>

namespace enigma::graphic
{
    /**
        * @brief Iris注释配置结构
        * 
        * 教学要点: 封装从着色器注释解析出的配置信息
        */
    struct IrisAnnotations
    {
        // 渲染目标配置
        std::vector<uint32_t> renderTargets; // RENDERTARGETS: 0,1,2,3
        std::string           drawBuffers; // DRAWBUFFERS: 0123 (兼容格式)

        // RT格式配置
        std::unordered_map<std::string, std::string>             rtFormats; // GAUX1FORMAT: RGBA32F
        std::unordered_map<std::string, std::pair<float, float>> rtSizes; // GAUX1SIZE: 0.5 0.5

        // 渲染状态配置
        std::optional<std::string> blendMode; // BLEND: SrcAlpha OneMinusSrcAlpha
        std::optional<std::string> depthTest; // DEPTHTEST: LessEqual
        std::optional<bool>        depthWrite; // DEPTHWRITE: false
        std::optional<std::string> cullFace; // CULLFACE: Back

        // 计算着色器配置
        std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeThreads; // COMPUTE_THREADS: 16,16,1
        std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> computeSize; // COMPUTE_SIZE: 1920,1080,1

        // 其他配置
        std::unordered_map<std::string, std::string> customDefines; // 自定义宏定义

        IrisAnnotations();
        void Reset();
    };

    /**
         * @brief 着色器程序类型枚举
         * 
         * 教学要点: 对应Iris规范的各种着色器程序类型
         */
    enum class ShaderType
    {
        // Setup阶段 - Compute Shader only
        Setup, // setup1.csh - setup99.csh

        // Begin阶段 - Composite-style
        Begin, // begin1.vsh/.fsh - begin99.vsh/.fsh

        // Shadow阶段 - Gbuffers-style
        Shadow, // shadow.vsh/.fsh

        // ShadowComp阶段 - Composite-style
        ShadowComp, // shadowcomp1.vsh/.fsh - shadowcomp99.vsh/.fsh

        // Prepare阶段 - Composite-style
        Prepare, // prepare1.vsh/.fsh - prepare99.vsh/.fsh

        // GBuffers阶段 - Gbuffers-style (18种不同程序)
        GBuffers_Terrain, // gbuffers_terrain.vsh/.fsh
        GBuffers_Entities, // gbuffers_entities.vsh/.fsh
        GBuffers_EntitiesTranslucent, // gbuffers_entities_translucent.vsh/.fsh
        GBuffers_Hand, // gbuffers_hand.vsh/.fsh
        GBuffers_Weather, // gbuffers_weather.vsh/.fsh
        GBuffers_Block, // gbuffers_block.vsh/.fsh
        GBuffers_BeaconBeam, // gbuffers_beaconbeam.vsh/.fsh
        GBuffers_Item, // gbuffers_item.vsh/.fsh
        GBuffers_Entities_glowing, // gbuffers_entities_glowing.vsh/.fsh
        GBuffers_Glint, // gbuffers_glint.vsh/.fsh
        GBuffers_Eyes, // gbuffers_eyes.vsh/.fsh
        GBuffers_Armor_glint, // gbuffers_armor_glint.vsh/.fsh
        GBuffers_SpiderEyes, // gbuffers_spidereyes.vsh/.fsh
        GBuffers_Hand_water, // gbuffers_hand_water.vsh/.fsh
        GBuffers_Textured, // gbuffers_textured.vsh/.fsh
        GBuffers_Textured_lit, // gbuffers_textured_lit.vsh/.fsh
        GBuffers_Skybasic, // gbuffers_skybasic.vsh/.fsh
        GBuffers_Skytextured, // gbuffers_skytextured.vsh/.fsh
        GBuffers_Clouds, // gbuffers_clouds.vsh/.fsh
        GBuffers_Water, // gbuffers_water.vsh/.fsh

        // Deferred阶段 - Composite-style
        Deferred, // deferred1.vsh/.fsh - deferred99.vsh/.fsh

        // Composite阶段 - Composite-style  
        Composite, // composite1.vsh/.fsh - composite99.vsh/.fsh

        // Final阶段 - Composite-style
        Final // final.vsh/.fsh
    };

    /**
     * @brief 着色器阶段枚举 (GPU流水线阶段)
     */
    enum class ShaderStage
    {
        Vertex, // 顶点着色器 (.vsh -> .hlsl)
        Pixel, // 像素着色器 (.fsh -> .hlsl)
        Compute, // 计算着色器 (.csh -> .hlsl)
        Geometry, // 几何着色器 (.gsh -> .hlsl) - 可选
        Hull, // 外壳着色器 (.tcs -> .hlsl) - 可选
        Domain // 域着色器 (.tes -> .hlsl) - 可选
    };

    /**
            * @brief 编译后的着色器程序
            */
    struct CompiledShader
    {
        ShaderType  type; // 着色器类型
        ShaderStage stage; // GPU流水线阶段
        std::string name; // 着色器名称
        std::string entryPoint; // 入口函数名
        std::string profile; // HLSL编译配置 (vs_5_0, ps_5_0等)

        ID3DBlob*            bytecode; // 编译后的字节码
        ID3D12PipelineState* pipelineState; // 管线状态对象 (如果已创建)

        IrisAnnotations annotations; // 解析的注释配置
        std::string     sourceCode; // 原始HLSL代码 (用于热重载)

        CompiledShader();
        ~CompiledShader();

        // 禁用拷贝
        CompiledShader(const CompiledShader&)            = delete;
        CompiledShader& operator=(const CompiledShader&) = delete;

        // 支持移动
        CompiledShader(CompiledShader&& other) noexcept;
        CompiledShader& operator=(CompiledShader&& other) noexcept;
    };
}
