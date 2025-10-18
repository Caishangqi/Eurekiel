// 教学要点: Windows头文件预处理宏，必须在所有头文件之前定义
#define WIN32_LEAN_AND_MEAN  // ⭐ 减少Windows.h包含内容，加速编译并避免冲突
#define NOMINMAX             // ⭐ 防止Windows.h定义min/max宏，避免与std::min/max冲突

#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"

#include <cstring>

// ⭐⭐⭐ 架构正确性: 使用D3D12RenderSystem静态API，遵循严格四层分层架构
// Layer 4 (UniformManager) → Layer 3 (D3D12RenderSystem) → Layer 2 (D12Buffer) → Layer 1 (DX12 Native)
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数 - RAII自动初始化
    // ========================================================================

    UniformManager::UniformManager()
    // 初始化所有11个CPU端结构体 ⭐
        : m_rootConstants()
          , m_cameraAndPlayerUniforms()
          , m_playerStatusUniforms()
          , m_screenAndSystemUniforms()
          , m_idUniforms()
          , m_worldAndWeatherUniforms()
          , m_biomeAndDimensionUniforms()
          , m_renderingUniforms()
          , m_matricesUniforms()
          , m_colorTargetsIndexBuffer()
          , m_depthTexturesIndexBuffer() // ⭐ 新增: depthtex0/1/2
          , m_shadowBufferIndex() // ⭐ 新增: shadowcolor + shadowtex
          // GPU资源指针初始化为nullptr
          , m_cameraAndPlayerBuffer(nullptr)
          , m_playerStatusBuffer(nullptr)
          , m_screenAndSystemBuffer(nullptr)
          , m_idBuffer(nullptr)
          , m_worldAndWeatherBuffer(nullptr)
          , m_biomeAndDimensionBuffer(nullptr)
          , m_renderingBuffer(nullptr)
          , m_matricesBuffer(nullptr)
          , m_colorTargetsBuffer(nullptr) // ⭐ 统一命名: colortex0-15
          , m_depthTexturesBuffer(nullptr) // ⭐ 新增
          , m_shadowBuffer(nullptr) // ⭐ 新增
          // 脏标记初始化为true (首次上传)
          , m_cameraAndPlayerDirty(true)
          , m_playerStatusDirty(true)
          , m_screenAndSystemDirty(true)
          , m_idDirty(true)
          , m_worldAndWeatherDirty(true)
          , m_biomeAndDimensionDirty(true)
          , m_renderingDirty(true)
          , m_matricesDirty(true)
          , m_renderTargetsDirty(true) // ⭐ 统一命名
          , m_depthTexturesDirty(true) // ⭐ 新增
          , m_shadowDirty(true) // ⭐ 新增
    {
        // 构建字段映射表
        BuildFieldMap();

        // 创建9个GPU StructuredBuffer
        CreateGPUBuffers();

        // 上传初始数据
        UploadDirtyBuffersToGPU();

        // 注册到Bindless系统
        RegisterToBindlessSystem();
    }

    // ========================================================================
    // 析构函数 - RAII自动释放
    // ========================================================================

    UniformManager::~UniformManager()
    {
        // 注销Bindless索引
        UnregisterFromBindlessSystem();

        // 释放GPU资源 (D12Buffer使用智能指针,自动释放)
        delete m_cameraAndPlayerBuffer;
        delete m_playerStatusBuffer;
        delete m_screenAndSystemBuffer;
        delete m_idBuffer;
        delete m_worldAndWeatherBuffer;
        delete m_biomeAndDimensionBuffer;
        delete m_renderingBuffer;
        delete m_matricesBuffer;
        delete m_colorTargetsBuffer; // ⭐ 统一命名: colortex0-15
        delete m_depthTexturesBuffer; // ⭐ 新增
        delete m_shadowBuffer; // ⭐ 新增
    }

    // ========================================================================
    // BuildFieldMap() - 构建字段映射表
    // ========================================================================

    void UniformManager::BuildFieldMap()
    {
        // 使用offsetof宏计算字段偏移量,构建unordered_map

        // ========================================================================
        // Category 0: CameraAndPlayerUniforms (16 fields)
        // ========================================================================
        m_fieldMap["cameraPosition"]              = {0, offsetof(CameraAndPlayerUniforms, cameraPosition), sizeof(Vec3)};
        m_fieldMap["eyeAltitude"]                 = {0, offsetof(CameraAndPlayerUniforms, eyeAltitude), sizeof(float)};
        m_fieldMap["cameraPositionFract"]         = {0, offsetof(CameraAndPlayerUniforms, cameraPositionFract), sizeof(Vec3)};
        m_fieldMap["cameraPositionInt"]           = {0, offsetof(CameraAndPlayerUniforms, cameraPositionInt), sizeof(IntVec3)};
        m_fieldMap["previousCameraPosition"]      = {0, offsetof(CameraAndPlayerUniforms, previousCameraPosition), sizeof(Vec3)};
        m_fieldMap["previousCameraPositionFract"] = {0, offsetof(CameraAndPlayerUniforms, previousCameraPositionFract), sizeof(Vec3)};
        m_fieldMap["previousCameraPositionInt"]   = {0, offsetof(CameraAndPlayerUniforms, previousCameraPositionInt), sizeof(IntVec3)};
        m_fieldMap["eyePosition"]                 = {0, offsetof(CameraAndPlayerUniforms, eyePosition), sizeof(Vec3)};
        m_fieldMap["relativeEyePosition"]         = {0, offsetof(CameraAndPlayerUniforms, relativeEyePosition), sizeof(Vec3)};
        m_fieldMap["playerBodyVector"]            = {0, offsetof(CameraAndPlayerUniforms, playerBodyVector), sizeof(Vec3)};
        m_fieldMap["playerLookVector"]            = {0, offsetof(CameraAndPlayerUniforms, playerLookVector), sizeof(Vec3)};
        m_fieldMap["upPosition"]                  = {0, offsetof(CameraAndPlayerUniforms, upPosition), sizeof(Vec3)};
        m_fieldMap["eyeBrightness"]               = {0, offsetof(CameraAndPlayerUniforms, eyeBrightness), sizeof(IntVec2)};
        m_fieldMap["eyeBrightnessSmooth"]         = {0, offsetof(CameraAndPlayerUniforms, eyeBrightnessSmooth), sizeof(IntVec2)};
        m_fieldMap["centerDepthSmooth"]           = {0, offsetof(CameraAndPlayerUniforms, centerDepthSmooth), sizeof(float)};
        m_fieldMap["firstPersonCamera"]           = {0, offsetof(CameraAndPlayerUniforms, firstPersonCamera), sizeof(bool)};

        // ========================================================================
        // Category 1: PlayerStatusUniforms (24 fields)
        // ========================================================================
        m_fieldMap["isEyeInWater"]        = {1, offsetof(PlayerStatusUniforms, isEyeInWater), sizeof(int)};
        m_fieldMap["isSpectator"]         = {1, offsetof(PlayerStatusUniforms, isSpectator), sizeof(bool)};
        m_fieldMap["isRightHanded"]       = {1, offsetof(PlayerStatusUniforms, isRightHanded), sizeof(bool)};
        m_fieldMap["blindness"]           = {1, offsetof(PlayerStatusUniforms, blindness), sizeof(float)};
        m_fieldMap["darknessFactor"]      = {1, offsetof(PlayerStatusUniforms, darknessFactor), sizeof(float)};
        m_fieldMap["darknessLightFactor"] = {1, offsetof(PlayerStatusUniforms, darknessLightFactor), sizeof(float)};
        m_fieldMap["nightVision"]         = {1, offsetof(PlayerStatusUniforms, nightVision), sizeof(float)};
        m_fieldMap["playerMood"]          = {1, offsetof(PlayerStatusUniforms, playerMood), sizeof(float)};
        m_fieldMap["constantMood"]        = {1, offsetof(PlayerStatusUniforms, constantMood), sizeof(float)};
        m_fieldMap["currentPlayerAir"]    = {1, offsetof(PlayerStatusUniforms, currentPlayerAir), sizeof(float)};
        m_fieldMap["maxPlayerAir"]        = {1, offsetof(PlayerStatusUniforms, maxPlayerAir), sizeof(float)};
        m_fieldMap["currentPlayerArmor"]  = {1, offsetof(PlayerStatusUniforms, currentPlayerArmor), sizeof(float)};
        m_fieldMap["maxPlayerArmor"]      = {1, offsetof(PlayerStatusUniforms, maxPlayerArmor), sizeof(float)};
        m_fieldMap["currentPlayerHealth"] = {1, offsetof(PlayerStatusUniforms, currentPlayerHealth), sizeof(float)};
        m_fieldMap["maxPlayerHealth"]     = {1, offsetof(PlayerStatusUniforms, maxPlayerHealth), sizeof(float)};
        m_fieldMap["currentPlayerHunger"] = {1, offsetof(PlayerStatusUniforms, currentPlayerHunger), sizeof(float)};
        m_fieldMap["maxPlayerHunger"]     = {1, offsetof(PlayerStatusUniforms, maxPlayerHunger), sizeof(float)};
        m_fieldMap["is_burning"]          = {1, offsetof(PlayerStatusUniforms, is_burning), sizeof(bool)};
        m_fieldMap["is_hurt"]             = {1, offsetof(PlayerStatusUniforms, is_hurt), sizeof(bool)};
        m_fieldMap["is_invisible"]        = {1, offsetof(PlayerStatusUniforms, is_invisible), sizeof(bool)};
        m_fieldMap["is_on_ground"]        = {1, offsetof(PlayerStatusUniforms, is_on_ground), sizeof(bool)};
        m_fieldMap["is_sneaking"]         = {1, offsetof(PlayerStatusUniforms, is_sneaking), sizeof(bool)};
        m_fieldMap["is_sprinting"]        = {1, offsetof(PlayerStatusUniforms, is_sprinting), sizeof(bool)};
        m_fieldMap["hideGUI"]             = {1, offsetof(PlayerStatusUniforms, hideGUI), sizeof(bool)};

        // ========================================================================
        // Category 2: ScreenAndSystemUniforms (11 fields)
        // ========================================================================
        m_fieldMap["viewHeight"]        = {2, offsetof(ScreenAndSystemUniforms, viewHeight), sizeof(float)};
        m_fieldMap["viewWidth"]         = {2, offsetof(ScreenAndSystemUniforms, viewWidth), sizeof(float)};
        m_fieldMap["aspectRatio"]       = {2, offsetof(ScreenAndSystemUniforms, aspectRatio), sizeof(float)};
        m_fieldMap["screenBrightness"]  = {2, offsetof(ScreenAndSystemUniforms, screenBrightness), sizeof(float)};
        m_fieldMap["frameCounter"]      = {2, offsetof(ScreenAndSystemUniforms, frameCounter), sizeof(uint32_t)};
        m_fieldMap["frameTime"]         = {2, offsetof(ScreenAndSystemUniforms, frameTime), sizeof(float)};
        m_fieldMap["frameTimeCounter"]  = {2, offsetof(ScreenAndSystemUniforms, frameTimeCounter), sizeof(float)};
        m_fieldMap["currentColorSpace"] = {2, offsetof(ScreenAndSystemUniforms, currentColorSpace), sizeof(uint32_t)};
        m_fieldMap["currentDate"]       = {2, offsetof(ScreenAndSystemUniforms, currentDate), sizeof(IntVec3)};
        m_fieldMap["currentTime"]       = {2, offsetof(ScreenAndSystemUniforms, currentTime), sizeof(IntVec3)};
        m_fieldMap["currentYearTime"]   = {2, offsetof(ScreenAndSystemUniforms, currentYearTime), sizeof(IntVec2)};

        // ========================================================================
        // Category 3: IDUniforms (9 fields)
        // ========================================================================
        m_fieldMap["entityId"]                = {3, offsetof(IDUniforms, entityId), sizeof(uint32_t)};
        m_fieldMap["blockEntityId"]           = {3, offsetof(IDUniforms, blockEntityId), sizeof(uint32_t)};
        m_fieldMap["currentRenderedItemId"]   = {3, offsetof(IDUniforms, currentRenderedItemId), sizeof(uint32_t)};
        m_fieldMap["currentSelectedBlockId"]  = {3, offsetof(IDUniforms, currentSelectedBlockId), sizeof(uint32_t)};
        m_fieldMap["currentSelectedBlockPos"] = {3, offsetof(IDUniforms, currentSelectedBlockPos), sizeof(Vec3)};
        m_fieldMap["heldItemId"]              = {3, offsetof(IDUniforms, heldItemId), sizeof(uint32_t)};
        m_fieldMap["heldItemId2"]             = {3, offsetof(IDUniforms, heldItemId2), sizeof(uint32_t)};
        m_fieldMap["heldBlockLightValue"]     = {3, offsetof(IDUniforms, heldBlockLightValue), sizeof(uint32_t)};
        m_fieldMap["heldBlockLightValue2"]    = {3, offsetof(IDUniforms, heldBlockLightValue2), sizeof(uint32_t)};

        // ========================================================================
        // Category 4: WorldAndWeatherUniforms (12 fields)
        // ========================================================================
        m_fieldMap["sunPosition"]           = {4, offsetof(WorldAndWeatherUniforms, sunPosition), sizeof(Vec3)};
        m_fieldMap["moonPosition"]          = {4, offsetof(WorldAndWeatherUniforms, moonPosition), sizeof(Vec3)};
        m_fieldMap["shadowLightPosition"]   = {4, offsetof(WorldAndWeatherUniforms, shadowLightPosition), sizeof(Vec3)};
        m_fieldMap["sunAngle"]              = {4, offsetof(WorldAndWeatherUniforms, sunAngle), sizeof(float)};
        m_fieldMap["shadowAngle"]           = {4, offsetof(WorldAndWeatherUniforms, shadowAngle), sizeof(float)};
        m_fieldMap["moonPhase"]             = {4, offsetof(WorldAndWeatherUniforms, moonPhase), sizeof(uint32_t)};
        m_fieldMap["rainStrength"]          = {4, offsetof(WorldAndWeatherUniforms, rainStrength), sizeof(float)};
        m_fieldMap["wetness"]               = {4, offsetof(WorldAndWeatherUniforms, wetness), sizeof(float)};
        m_fieldMap["thunderStrength"]       = {4, offsetof(WorldAndWeatherUniforms, thunderStrength), sizeof(float)};
        m_fieldMap["lightningBoltPosition"] = {4, offsetof(WorldAndWeatherUniforms, lightningBoltPosition), sizeof(Vec4)};
        m_fieldMap["worldTime"]             = {4, offsetof(WorldAndWeatherUniforms, worldTime), sizeof(uint32_t)};
        m_fieldMap["worldDay"]              = {4, offsetof(WorldAndWeatherUniforms, worldDay), sizeof(uint32_t)};

        // ========================================================================
        // Category 5: BiomeAndDimensionUniforms (12 fields)
        // ========================================================================
        m_fieldMap["biome"]               = {5, offsetof(BiomeAndDimensionUniforms, biome), sizeof(uint32_t)};
        m_fieldMap["biome_category"]      = {5, offsetof(BiomeAndDimensionUniforms, biome_category), sizeof(uint32_t)};
        m_fieldMap["biome_precipitation"] = {5, offsetof(BiomeAndDimensionUniforms, biome_precipitation), sizeof(uint32_t)};
        m_fieldMap["rainfall"]            = {5, offsetof(BiomeAndDimensionUniforms, rainfall), sizeof(float)};
        m_fieldMap["temperature"]         = {5, offsetof(BiomeAndDimensionUniforms, temperature), sizeof(float)};
        m_fieldMap["ambientLight"]        = {5, offsetof(BiomeAndDimensionUniforms, ambientLight), sizeof(float)};
        m_fieldMap["bedrockLevel"]        = {5, offsetof(BiomeAndDimensionUniforms, bedrockLevel), sizeof(uint32_t)};
        m_fieldMap["cloudHeight"]         = {5, offsetof(BiomeAndDimensionUniforms, cloudHeight), sizeof(float)};
        m_fieldMap["hasCeiling"]          = {5, offsetof(BiomeAndDimensionUniforms, hasCeiling), sizeof(uint32_t)};
        m_fieldMap["hasSkylight"]         = {5, offsetof(BiomeAndDimensionUniforms, hasSkylight), sizeof(uint32_t)};
        m_fieldMap["heightLimit"]         = {5, offsetof(BiomeAndDimensionUniforms, heightLimit), sizeof(uint32_t)};
        m_fieldMap["logicalHeightLimit"]  = {5, offsetof(BiomeAndDimensionUniforms, logicalHeightLimit), sizeof(uint32_t)};
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif
        // ========================================================================
        // Category 6: RenderingUniforms (15 fields)
        // ========================================================================
        m_fieldMap["near"]         = {6, offsetof(RenderingUniforms, near), sizeof(float)};
        m_fieldMap["far"]          = {6, offsetof(RenderingUniforms, far), sizeof(float)};
        m_fieldMap["alphaTestRef"] = {6, offsetof(RenderingUniforms, alphaTestRef), sizeof(float)};
        m_fieldMap["chunkOffset"]  = {6, offsetof(RenderingUniforms, chunkOffset), sizeof(Vec3)};
        m_fieldMap["entityColor"]  = {6, offsetof(RenderingUniforms, entityColor), sizeof(Vec4)};
        m_fieldMap["blendFunc"]    = {6, offsetof(RenderingUniforms, blendFunc), sizeof(IntVec4)};
        m_fieldMap["atlasSize"]    = {6, offsetof(RenderingUniforms, atlasSize), sizeof(IntVec2)};
        m_fieldMap["renderStage"]  = {6, offsetof(RenderingUniforms, renderStage), sizeof(int32_t)};
        m_fieldMap["fogColor"]     = {6, offsetof(RenderingUniforms, fogColor), sizeof(Vec3)};
        m_fieldMap["skyColor"]     = {6, offsetof(RenderingUniforms, skyColor), sizeof(Vec3)};
        m_fieldMap["fogDensity"]   = {6, offsetof(RenderingUniforms, fogDensity), sizeof(float)};
        m_fieldMap["fogStart"]     = {6, offsetof(RenderingUniforms, fogStart), sizeof(float)};
        m_fieldMap["fogEnd"]       = {6, offsetof(RenderingUniforms, fogEnd), sizeof(float)};
        m_fieldMap["fogMode"]      = {6, offsetof(RenderingUniforms, fogMode), sizeof(int32_t)};
        m_fieldMap["fogShape"]     = {6, offsetof(RenderingUniforms, fogShape), sizeof(int32_t)};

        // ========================================================================
        // Category 7: MatricesUniforms (16 Mat44矩阵)
        // ========================================================================
        m_fieldMap["gbufferModelView"]          = {7, offsetof(MatricesUniforms, gbufferModelView), sizeof(Mat44)};
        m_fieldMap["gbufferModelViewInverse"]   = {7, offsetof(MatricesUniforms, gbufferModelViewInverse), sizeof(Mat44)};
        m_fieldMap["gbufferProjection"]         = {7, offsetof(MatricesUniforms, gbufferProjection), sizeof(Mat44)};
        m_fieldMap["gbufferProjectionInverse"]  = {7, offsetof(MatricesUniforms, gbufferProjectionInverse), sizeof(Mat44)};
        m_fieldMap["gbufferPreviousModelView"]  = {7, offsetof(MatricesUniforms, gbufferPreviousModelView), sizeof(Mat44)};
        m_fieldMap["gbufferPreviousProjection"] = {7, offsetof(MatricesUniforms, gbufferPreviousProjection), sizeof(Mat44)};
        m_fieldMap["shadowModelView"]           = {7, offsetof(MatricesUniforms, shadowModelView), sizeof(Mat44)};
        m_fieldMap["shadowModelViewInverse"]    = {7, offsetof(MatricesUniforms, shadowModelViewInverse), sizeof(Mat44)};
        m_fieldMap["shadowProjection"]          = {7, offsetof(MatricesUniforms, shadowProjection), sizeof(Mat44)};
        m_fieldMap["shadowProjectionInverse"]   = {7, offsetof(MatricesUniforms, shadowProjectionInverse), sizeof(Mat44)};
        m_fieldMap["modelViewMatrix"]           = {7, offsetof(MatricesUniforms, modelViewMatrix), sizeof(Mat44)};
        m_fieldMap["modelViewMatrixInverse"]    = {7, offsetof(MatricesUniforms, modelViewMatrixInverse), sizeof(Mat44)};
        m_fieldMap["projectionMatrix"]          = {7, offsetof(MatricesUniforms, projectionMatrix), sizeof(Mat44)};
        m_fieldMap["projectionMatrixInverse"]   = {7, offsetof(MatricesUniforms, projectionMatrixInverse), sizeof(Mat44)};
        m_fieldMap["normalMatrix"]              = {7, offsetof(MatricesUniforms, normalMatrix), sizeof(Mat44)};
        m_fieldMap["textureMatrix"]             = {7, offsetof(MatricesUniforms, textureMatrix), sizeof(Mat44)};

        // ========================================================================
        // Category 8: ColorTargetsIndexBuffer (2 fields)
        // ========================================================================
        // 教学要点: colortex0-15 的读写索引数组
        // 注意: 这些字段不常通过Uniform1i访问,而是通过专用API (FlipRenderTargets, UpdateRenderTargetsReadIndices)
        m_fieldMap["readIndices"]  = {8, offsetof(ColorTargetsIndexBuffer, readIndices), sizeof(uint32_t[16])};
        m_fieldMap["writeIndices"] = {8, offsetof(ColorTargetsIndexBuffer, writeIndices), sizeof(uint32_t[16])};

        // ========================================================================
        // Category 9: DepthTexturesIndexBuffer (3 fields)
        // ========================================================================
        // 教学要点: depthtex0/1/2 索引管理
        m_fieldMap["depthtex0Index"] = {9, offsetof(DepthTexturesIndexBuffer, depthtex0Index), sizeof(uint32_t)};
        m_fieldMap["depthtex1Index"] = {9, offsetof(DepthTexturesIndexBuffer, depthtex1Index), sizeof(uint32_t)};
        m_fieldMap["depthtex2Index"] = {9, offsetof(DepthTexturesIndexBuffer, depthtex2Index), sizeof(uint32_t)};

        // ========================================================================
        // Category 10: ShadowBufferIndex (18 fields)
        // ========================================================================
        // 教学要点: shadowcolor0-7 + shadowtex0/1 索引管理
        m_fieldMap["shadowColorReadIndices"]  = {10, offsetof(ShadowBufferIndex, shadowColorReadIndices), sizeof(uint32_t[8])};
        m_fieldMap["shadowColorWriteIndices"] = {10, offsetof(ShadowBufferIndex, shadowColorWriteIndices), sizeof(uint32_t[8])};
        m_fieldMap["shadowtex0Index"]         = {10, offsetof(ShadowBufferIndex, shadowtex0Index), sizeof(uint32_t)};
        m_fieldMap["shadowtex1Index"]         = {10, offsetof(ShadowBufferIndex, shadowtex1Index), sizeof(uint32_t)};
    }

    // ========================================================================
    // CreateGPUBuffers() - 创建11个GPU StructuredBuffer ⭐
    // ========================================================================

    void UniformManager::CreateGPUBuffers()
    {
        // ⭐⭐⭐ 教学要点: 遵循严格四层架构
        // Layer 4 (UniformManager) → Layer 3 (D3D12RenderSystem::CreateStructuredBuffer)
        // 参数顺序: (elementCount, elementSize, initialData, debugName)
        // 返回值: std::unique_ptr<D12Buffer> → 调用 .release() 转换为裸指针

        // Category 0: CameraAndPlayerUniforms (~112 bytes)
        m_cameraAndPlayerBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1, // elementCount (单个元素)
            sizeof(CameraAndPlayerUniforms), // elementSize
            nullptr, // initialData (稍后上传)
            "CameraAndPlayerBuffer" // debugName (char* not wchar_t*)
        ).release(); // + unique_ptr → raw pointer

        // Category 1: PlayerStatusUniforms (~80 bytes)
        m_playerStatusBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(PlayerStatusUniforms),
            nullptr,
            "PlayerStatusBuffer"
        ).release();

        // Category 2: ScreenAndSystemUniforms (~96 bytes)
        m_screenAndSystemBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(ScreenAndSystemUniforms),
            nullptr,
            "ScreenAndSystemBuffer"
        ).release();

        // Category 3: IDUniforms (~64 bytes)
        m_idBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(IDUniforms),
            nullptr,
            "IDBuffer"
        ).release();

        // Category 4: WorldAndWeatherUniforms (~176 bytes)
        m_worldAndWeatherBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(WorldAndWeatherUniforms),
            nullptr,
            "WorldAndWeatherBuffer"
        ).release();

        // Category 5: BiomeAndDimensionUniforms (~160 bytes)
        m_biomeAndDimensionBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(BiomeAndDimensionUniforms),
            nullptr,
            "BiomeAndDimensionBuffer"
        ).release();

        // Category 6: RenderingUniforms (~176 bytes)
        m_renderingBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(RenderingUniforms),
            nullptr,
            "RenderingBuffer"
        ).release();

        // Category 7: MatricesUniforms (1152 bytes - 16个Mat44矩阵)
        m_matricesBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(MatricesUniforms),
            nullptr,
            "MatricesBuffer"
        ).release();

        // Category 8: ColorTargetsIndexBuffer (128 bytes - colortex0-15)
        m_colorTargetsBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(ColorTargetsIndexBuffer),
            nullptr,
            "ColorTargetsIndexBuffer"
        ).release();

        // Category 9: DepthTexturesIndexBuffer (16 bytes - depthtex0/1/2) ⭐
        m_depthTexturesBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(DepthTexturesIndexBuffer),
            nullptr,
            "DepthTexturesIndexBuffer"
        ).release();

        // Category 10: ShadowBufferIndex (80 bytes - shadowcolor + shadowtex) ⭐
        m_shadowBuffer = D3D12RenderSystem::CreateStructuredBuffer(
            1,
            sizeof(ShadowBufferIndex),
            nullptr,
            "ShadowBufferIndex"
        ).release();
    }

    // ========================================================================
    // RegisterToBindlessSystem() - 注册11个Buffer到Bindless系统 ⭐
    // ========================================================================

    void UniformManager::RegisterToBindlessSystem()
    {
        // 教学要点: 新架构使用D12Buffer::RegisterBindless()自动获取Bindless组件
        // 每个buffer自己管理注册，返回std::optional<uint32_t>

        // 注册8个Uniform buffers (Category 0-7)
        auto index0 = m_cameraAndPlayerBuffer->RegisterBindless();
        auto index1 = m_playerStatusBuffer->RegisterBindless();
        auto index2 = m_screenAndSystemBuffer->RegisterBindless();
        auto index3 = m_idBuffer->RegisterBindless();
        auto index4 = m_worldAndWeatherBuffer->RegisterBindless();
        auto index5 = m_biomeAndDimensionBuffer->RegisterBindless();
        auto index6 = m_renderingBuffer->RegisterBindless();
        auto index7 = m_matricesBuffer->RegisterBindless();

        // 注册3个纹理系统 Index Buffers (Category 8-10) ⭐
        auto index8  = m_colorTargetsBuffer->RegisterBindless(); // colortex0-15
        auto index9  = m_depthTexturesBuffer->RegisterBindless(); // depthtex0/1/2
        auto index10 = m_shadowBuffer->RegisterBindless(); // shadowcolor + shadowtex

        // 验证并存储索引到Root Constants
        if (!index0 || !index1 || !index2 || !index3 || !index4 ||
            !index5 || !index6 || !index7 || !index8 || !index9 || !index10)
        {
            ERROR_AND_DIE("UniformManager::RegisterToBindlessSystem - Failed to register buffers to Bindless system");
        }

        m_rootConstants.cameraAndPlayerBufferIndex   = *index0;
        m_rootConstants.playerStatusBufferIndex      = *index1;
        m_rootConstants.screenAndSystemBufferIndex   = *index2;
        m_rootConstants.idBufferIndex                = *index3;
        m_rootConstants.worldAndWeatherBufferIndex   = *index4;
        m_rootConstants.biomeAndDimensionBufferIndex = *index5;
        m_rootConstants.renderingBufferIndex         = *index6;
        m_rootConstants.matricesBufferIndex          = *index7;
        m_rootConstants.colorTargetsBufferIndex      = *index8;
        m_rootConstants.depthTexturesBufferIndex     = *index9;
        m_rootConstants.shadowBufferIndex            = *index10;

        // 注意: noiseTextureIndex 不在这里注册,它是直接的纹理索引,由外部设置
        // 通过 SetNoiseTextureIndex() API 设置
    }

    // ========================================================================
    // UnregisterFromBindlessSystem() - 注销11个Buffer ⭐
    // ========================================================================

    void UniformManager::UnregisterFromBindlessSystem()
    {
        // 教学要点: 新架构使用D12Buffer::UnregisterBindless()自动释放索引
        // 每个buffer自己管理注销，RAII设计确保安全

        // 注销8个Uniform buffers (Category 0-7)
        m_cameraAndPlayerBuffer->UnregisterBindless();
        m_playerStatusBuffer->UnregisterBindless();
        m_screenAndSystemBuffer->UnregisterBindless();
        m_idBuffer->UnregisterBindless();
        m_worldAndWeatherBuffer->UnregisterBindless();
        m_biomeAndDimensionBuffer->UnregisterBindless();
        m_renderingBuffer->UnregisterBindless();
        m_matricesBuffer->UnregisterBindless();

        // 注销3个纹理系统 Index Buffers (Category 8-10) ⭐
        m_colorTargetsBuffer->UnregisterBindless();
        m_depthTexturesBuffer->UnregisterBindless();
        m_shadowBuffer->UnregisterBindless();

        // 注意: noiseTextureIndex 不需要注销,它是外部管理的纹理索引
    }

    // ========================================================================
    // Fluent Builder API - Supplier版本 (引擎内部使用)
    // ========================================================================

    UniformManager& UniformManager::Uniform1f(const std::string& name, std::function<float()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            float value = getter();
            std::memcpy(dest, &value, sizeof(float));
        });
    }

    UniformManager& UniformManager::Uniform1i(const std::string& name, std::function<int32_t()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            int32_t value = getter();
            std::memcpy(dest, &value, sizeof(int32_t));
        });
    }

    UniformManager& UniformManager::Uniform1b(const std::string& name, std::function<bool()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            int32_t value = getter() ? 1 : 0; // bool -> int32_t
            std::memcpy(dest, &value, sizeof(int32_t));
        });
    }

    UniformManager& UniformManager::Uniform2f(const std::string& name, std::function<Vec2()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            Vec2 value = getter();
            std::memcpy(dest, &value, sizeof(Vec2));
        });
    }

    UniformManager& UniformManager::Uniform2i(const std::string& name, std::function<IntVec2()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            IntVec2 value = getter();
            std::memcpy(dest, &value, sizeof(IntVec2));
        });
    }

    UniformManager& UniformManager::Uniform3f(const std::string& name, std::function<Vec3()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            Vec3 value = getter();
            std::memcpy(dest, &value, sizeof(Vec3));
        });
    }

    UniformManager& UniformManager::Uniform3i(const std::string& name, std::function<IntVec3()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            IntVec3 value = getter();
            std::memcpy(dest, &value, sizeof(IntVec3));
        });
    }

    UniformManager& UniformManager::Uniform4f(const std::string& name, std::function<Vec4()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            Vec4 value = getter();
            std::memcpy(dest, &value, sizeof(Vec4));
        });
    }

    UniformManager& UniformManager::Uniform4i(const std::string& name, std::function<IntVec4()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            IntVec4 value = getter();
            std::memcpy(dest, &value, sizeof(IntVec4));
        });
    }

    UniformManager& UniformManager::UniformMat4(const std::string& name, std::function<Mat44()> getter)
    {
        return RegisterGetter(name, [getter = std::move(getter)](void* dest)
        {
            Mat44 value = getter();
            std::memcpy(dest, &value, sizeof(Mat44));
        });
    }

    // ========================================================================
    // Fluent Builder API - 直接值版本 (游戏侧使用)
    // ========================================================================

    UniformManager& UniformManager::Uniform1f(const std::string& name, float value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform1f - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(float));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform1i(const std::string& name, int32_t value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform1i - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(int32_t));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform1b(const std::string& name, bool value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform1b - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        int32_t          intValue     = value ? 1 : 0; // bool -> int32_t
        std::memcpy(fieldPtr, &intValue, sizeof(int32_t));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform2f(const std::string& name, const Vec2& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform2f - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(Vec2));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform2i(const std::string& name, const IntVec2& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform2i - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(IntVec2));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform3f(const std::string& name, const Vec3& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform3f - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(Vec3));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform3i(const std::string& name, const IntVec3& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform3i - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(IntVec3));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform4f(const std::string& name, const Vec4& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform4f - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(Vec4));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::Uniform4i(const std::string& name, const IntVec4& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::Uniform4i - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(IntVec4));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    UniformManager& UniformManager::UniformMat4(const std::string& name, const Mat44& value)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::UniformMat4 - Field not found: " + name);
        }

        const FieldInfo& info         = it->second;
        void*            categoryData = GetCategoryDataPtr(info.categoryIndex);
        void*            fieldPtr     = static_cast<char*>(categoryData) + info.offset;
        std::memcpy(fieldPtr, &value, sizeof(Mat44));

        MarkCategoryDirty(info.categoryIndex);
        return *this;
    }

    // ========================================================================
    // RegisterGetter() - 通用Getter注册 (Supplier模式核心)
    // ========================================================================

    UniformManager& UniformManager::RegisterGetter(const std::string&         name,
                                                   std::function<void(void*)> getter)
    {
        auto it = m_fieldMap.find(name);
        if (it == m_fieldMap.end())
        {
            ERROR_AND_DIE("UniformManager::RegisterGetter - Field not found: " + name);
        }

        const FieldInfo& info = it->second;

        // 添加到Getter列表
        UniformGetter uniformGetter;
        uniformGetter.categoryIndex = info.categoryIndex;
        uniformGetter.offset        = info.offset;
        uniformGetter.getter        = std::move(getter);

        m_uniformGetters.push_back(uniformGetter);

        return *this;
    }

    // ========================================================================
    // ColorTargetsIndexBuffer 专用API
    // ========================================================================

    void UniformManager::UpdateRenderTargetsReadIndices(const uint32_t readIndices[16])
    {
        std::memcpy(m_colorTargetsIndexBuffer.readIndices, readIndices, sizeof(uint32_t) * 16);
        m_renderTargetsDirty = true; // ⭐ 统一命名
    }

    void UniformManager::UpdateRenderTargetsWriteIndices(const uint32_t writeIndices[16])
    {
        std::memcpy(m_colorTargetsIndexBuffer.writeIndices, writeIndices, sizeof(uint32_t) * 16);
        m_renderTargetsDirty = true; // ⭐ 统一命名
    }

    void UniformManager::FlipRenderTargets(const uint32_t mainIndices[16],
                                           const uint32_t altIndices[16],
                                           bool           useAlt)
    {
        if (useAlt)
        {
            // 读取Alt, 写入Main
            std::memcpy(m_colorTargetsIndexBuffer.readIndices, altIndices, sizeof(uint32_t) * 16);
            std::memcpy(m_colorTargetsIndexBuffer.writeIndices, mainIndices, sizeof(uint32_t) * 16);
        }
        else
        {
            // 读取Main, 写入Alt
            std::memcpy(m_colorTargetsIndexBuffer.readIndices, mainIndices, sizeof(uint32_t) * 16);
            std::memcpy(m_colorTargetsIndexBuffer.writeIndices, altIndices, sizeof(uint32_t) * 16);
        }

        m_renderTargetsDirty = true;
    }

    // ========================================================================
    // SyncToGPU() - 执行所有Supplier并上传脏Buffer
    // ========================================================================

    void UniformManager::SetShadowTexIndices(uint32_t shadowtex0, uint32_t shadowtex1)
    {
        UNUSED(shadowtex0)
        UNUSED(shadowtex1)
    }

    bool UniformManager::SyncToGPU()
    {
        // 1. 执行所有Supplier (更新CPU端数据)
        for (const auto& uniformGetter : m_uniformGetters)
        {
            void* categoryData = GetCategoryDataPtr(uniformGetter.categoryIndex);
            void* fieldPtr     = static_cast<char*>(categoryData) + uniformGetter.offset;
            uniformGetter.getter(fieldPtr); // 调用lambda

            // 标记脏
            MarkCategoryDirty(uniformGetter.categoryIndex);
        }

        // 2. 上传所有脏Buffer到GPU
        return UploadDirtyBuffersToGPU();
    }

    // ========================================================================
    // GetCategoryDataPtr() - 获取指定类别的CPU端数据指针
    // ========================================================================

    void* UniformManager::GetCategoryDataPtr(uint8_t categoryIndex)
    {
        switch (categoryIndex)
        {
        case 0: return &m_cameraAndPlayerUniforms;
        case 1: return &m_playerStatusUniforms;
        case 2: return &m_screenAndSystemUniforms;
        case 3: return &m_idUniforms;
        case 4: return &m_worldAndWeatherUniforms;
        case 5: return &m_biomeAndDimensionUniforms;
        case 6: return &m_renderingUniforms;
        case 7: return &m_matricesUniforms;
        case 8: return &m_colorTargetsIndexBuffer;
        case 9: return &m_depthTexturesIndexBuffer;
        case 10: return &m_shadowBufferIndex;
        default:
            ERROR_AND_DIE("UniformManager::GetCategoryDataPtr - Invalid categoryIndex: " + std::to_string(categoryIndex));
        }
    }

    // ========================================================================
    // MarkCategoryDirty() - 标记指定类别为脏
    // ========================================================================

    void UniformManager::MarkCategoryDirty(uint8_t categoryIndex)
    {
        switch (categoryIndex)
        {
        case 0: m_cameraAndPlayerDirty = true;
            break;
        case 1: m_playerStatusDirty = true;
            break;
        case 2: m_screenAndSystemDirty = true;
            break;
        case 3: m_idDirty = true;
            break;
        case 4: m_worldAndWeatherDirty = true;
            break;
        case 5: m_biomeAndDimensionDirty = true;
            break;
        case 6: m_renderingDirty = true;
            break;
        case 7: m_matricesDirty = true;
            break;
        case 8: m_renderTargetsDirty = true;
            break;
        case 9: m_depthTexturesDirty = true; // ⭐ 新增: depthtex0/1/2
            break;
        case 10: m_shadowDirty = true; // ⭐ 新增: shadowcolor + shadowtex
            break;
        default:
            ERROR_AND_DIE("UniformManager::MarkCategoryDirty - Invalid categoryIndex: " + std::to_string(categoryIndex));
        }
    }

    // ========================================================================
    // UploadDirtyBuffersToGPU() - 上传所有脏Buffer到GPU
    // ========================================================================

    bool UniformManager::UploadDirtyBuffersToGPU()
    {
        // ⭐⭐⭐ 教学要点: 使用 D12Resource::Upload() 基类方法
        // 正确模式: SetInitialData() → Upload()
        // 遵循严格四层架构和 Template Method 设计模式

        // Category 0: CameraAndPlayerUniforms (~112 bytes)
        if (m_cameraAndPlayerDirty)
        {
            m_cameraAndPlayerBuffer->SetInitialData(&m_cameraAndPlayerUniforms, sizeof(CameraAndPlayerUniforms));
            if (!m_cameraAndPlayerBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload CameraAndPlayerBuffer to GPU");
            }
            m_cameraAndPlayerDirty = false;
        }

        // Category 1: PlayerStatusUniforms (~80 bytes)
        if (m_playerStatusDirty)
        {
            m_playerStatusBuffer->SetInitialData(&m_playerStatusUniforms, sizeof(PlayerStatusUniforms));
            if (!m_playerStatusBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload PlayerStatusBuffer to GPU");
            }
            m_playerStatusDirty = false;
        }

        // Category 2: ScreenAndSystemUniforms (~96 bytes)
        if (m_screenAndSystemDirty)
        {
            m_screenAndSystemBuffer->SetInitialData(&m_screenAndSystemUniforms, sizeof(ScreenAndSystemUniforms));
            if (!m_screenAndSystemBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload ScreenAndSystemBuffer to GPU");
            }
            m_screenAndSystemDirty = false;
        }

        // Category 3: IDUniforms (~64 bytes)
        if (m_idDirty)
        {
            m_idBuffer->SetInitialData(&m_idUniforms, sizeof(IDUniforms));
            if (!m_idBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload IDBuffer to GPU");
            }
            m_idDirty = false;
        }

        // Category 4: WorldAndWeatherUniforms (~176 bytes)
        if (m_worldAndWeatherDirty)
        {
            m_worldAndWeatherBuffer->SetInitialData(&m_worldAndWeatherUniforms, sizeof(WorldAndWeatherUniforms));
            if (!m_worldAndWeatherBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload WorldAndWeatherBuffer to GPU");
            }
            m_worldAndWeatherDirty = false;
        }

        // Category 5: BiomeAndDimensionUniforms (~160 bytes)
        if (m_biomeAndDimensionDirty)
        {
            m_biomeAndDimensionBuffer->SetInitialData(&m_biomeAndDimensionUniforms, sizeof(BiomeAndDimensionUniforms));
            if (!m_biomeAndDimensionBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload BiomeAndDimensionBuffer to GPU");
            }
            m_biomeAndDimensionDirty = false;
        }

        // Category 6: RenderingUniforms (~176 bytes)
        if (m_renderingDirty)
        {
            m_renderingBuffer->SetInitialData(&m_renderingUniforms, sizeof(RenderingUniforms));
            if (!m_renderingBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload RenderingBuffer to GPU");
            }
            m_renderingDirty = false;
        }

        // Category 7: MatricesUniforms (1152 bytes - 16个Mat44矩阵)
        if (m_matricesDirty)
        {
            m_matricesBuffer->SetInitialData(&m_matricesUniforms, sizeof(MatricesUniforms));
            if (!m_matricesBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload MatricesBuffer to GPU");
            }
            m_matricesDirty = false;
        }

        // Category 8: ColorTargetsIndexBuffer (128 bytes - colortex0-15) ⭐
        if (m_renderTargetsDirty)
        {
            // 教学要点: m_renderTargetsBuffer是GPU buffer指针, m_renderTargetsIndexBuffer是CPU数据
            m_colorTargetsBuffer->SetInitialData(&m_colorTargetsIndexBuffer, sizeof(ColorTargetsIndexBuffer));
            if (!m_colorTargetsBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload RenderTargetsBuffer to GPU");
            }
            m_renderTargetsDirty = false;
        }

        // Category 9: DepthTexturesIndexBuffer (16 bytes - depthtex0/1/2) ⭐
        if (m_depthTexturesDirty)
        {
            m_depthTexturesBuffer->SetInitialData(&m_depthTexturesIndexBuffer, sizeof(DepthTexturesIndexBuffer));
            if (!m_depthTexturesBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload DepthTexturesBuffer to GPU");
            }
            m_depthTexturesDirty = false;
        }

        // Category 10: ShadowBufferIndex (80 bytes - shadowcolor + shadowtex) ⭐
        if (m_shadowDirty)
        {
            m_shadowBuffer->SetInitialData(&m_shadowBufferIndex, sizeof(ShadowBufferIndex));
            if (!m_shadowBuffer->Upload())
            {
                ERROR_AND_DIE("UniformManager: Failed to upload ShadowBuffer to GPU");
            }
            m_shadowDirty = false;
        }

        return true;
    }

    // ========================================================================
    // 数据访问接口
    // ========================================================================

    uint32_t UniformManager::GetCameraAndPlayerBufferIndex() const
    {
        return m_rootConstants.cameraAndPlayerBufferIndex;
    }

    uint32_t UniformManager::GetPlayerStatusBufferIndex() const
    {
        return m_rootConstants.playerStatusBufferIndex;
    }

    uint32_t UniformManager::GetScreenAndSystemBufferIndex() const
    {
        return m_rootConstants.screenAndSystemBufferIndex;
    }

    uint32_t UniformManager::GetIDBufferIndex() const
    {
        return m_rootConstants.idBufferIndex;
    }

    uint32_t UniformManager::GetWorldAndWeatherBufferIndex() const
    {
        return m_rootConstants.worldAndWeatherBufferIndex;
    }

    uint32_t UniformManager::GetBiomeAndDimensionBufferIndex() const
    {
        return m_rootConstants.biomeAndDimensionBufferIndex;
    }

    uint32_t UniformManager::GetRenderingBufferIndex() const
    {
        return m_rootConstants.renderingBufferIndex;
    }

    uint32_t UniformManager::GetMatricesBufferIndex() const
    {
        return m_rootConstants.matricesBufferIndex;
    }

    uint32_t UniformManager::GetColorTargetsBufferIndex() const
    {
        return m_rootConstants.colorTargetsBufferIndex;
    }

    uint32_t UniformManager::GetDepthTexturesBufferIndex() const
    {
        return m_rootConstants.depthTexturesBufferIndex;
    }

    uint32_t UniformManager::GetShadowBufferIndex() const
    {
        return m_rootConstants.shadowBufferIndex;
    }

    // ========================================================================
    // Reset() - 重置为默认值
    // ========================================================================

    void UniformManager::Reset()
    {
        // 重置所有11个CPU端结构体为默认值 ⭐
        m_cameraAndPlayerUniforms   = CameraAndPlayerUniforms();
        m_playerStatusUniforms      = PlayerStatusUniforms();
        m_screenAndSystemUniforms   = ScreenAndSystemUniforms();
        m_idUniforms                = IDUniforms();
        m_worldAndWeatherUniforms   = WorldAndWeatherUniforms();
        m_biomeAndDimensionUniforms = BiomeAndDimensionUniforms();
        m_renderingUniforms         = RenderingUniforms();
        m_matricesUniforms          = MatricesUniforms();
        m_colorTargetsIndexBuffer   = ColorTargetsIndexBuffer();
        m_depthTexturesIndexBuffer  = DepthTexturesIndexBuffer(); // ⭐ 新增
        m_shadowBufferIndex         = ShadowBufferIndex(); // ⭐ 新增

        // 标记所有11个类别为脏 ⭐
        m_cameraAndPlayerDirty   = true;
        m_playerStatusDirty      = true;
        m_screenAndSystemDirty   = true;
        m_idDirty                = true;
        m_worldAndWeatherDirty   = true;
        m_biomeAndDimensionDirty = true;
        m_renderingDirty         = true;
        m_matricesDirty          = true;
        m_renderTargetsDirty     = true; // ⭐ 统一命名
        m_depthTexturesDirty     = true; // ⭐ 新增
        m_shadowDirty            = true; // ⭐ 新增

        // 清空Supplier列表
        m_uniformGetters.clear();
    }
} // namespace enigma::graphic
