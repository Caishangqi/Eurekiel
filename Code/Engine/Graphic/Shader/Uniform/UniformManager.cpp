#include "UniformManager.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Resource/BindlessResourceManager.hpp"

#include <cstring>

#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造函数 - RAII自动初始化
    // ========================================================================

    UniformManager::UniformManager()
    // 初始化所有9个CPU端结构体
        : m_rootConstants()
          , m_cameraAndPlayerUniforms()
          , m_playerStatusUniforms()
          , m_screenAndSystemUniforms()
          , m_idUniforms()
          , m_worldAndWeatherUniforms()
          , m_biomeAndDimensionUniforms()
          , m_renderingUniforms()
          , m_matricesUniforms()
          , m_renderTargetsIndexBuffer()
          // GPU资源指针初始化为nullptr
          , m_cameraAndPlayerBuffer(nullptr)
          , m_playerStatusBuffer(nullptr)
          , m_screenAndSystemBuffer(nullptr)
          , m_idBuffer(nullptr)
          , m_worldAndWeatherBuffer(nullptr)
          , m_biomeAndDimensionBuffer(nullptr)
          , m_renderingBuffer(nullptr)
          , m_matricesBuffer(nullptr)
          // 脏标记初始化为true (首次上传)
          , m_cameraAndPlayerDirty(true)
          , m_playerStatusDirty(true)
          , m_screenAndSystemDirty(true)
          , m_idDirty(true)
          , m_worldAndWeatherDirty(true)
          , m_biomeAndDimensionDirty(true)
          , m_renderingDirty(true)
          , m_matricesDirty(true)
          , m_renderTargetsDirty(true)
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
        delete m_renderTargetsIndexBuffer;
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
        // Category 8: RenderTargetsIndexBuffer (9 fields)
        // ========================================================================
        m_fieldMap["readIndices"]       = {8, offsetof(RenderTargetsIndexBuffer, readIndices), sizeof(int[16])};
        m_fieldMap["writeIndices"]      = {8, offsetof(RenderTargetsIndexBuffer, writeIndices), sizeof(int[16])};
        m_fieldMap["depthtex0Index"]    = {8, offsetof(RenderTargetsIndexBuffer, depthtex0Index), sizeof(uint32_t)};
        m_fieldMap["depthtex1Index"]    = {8, offsetof(RenderTargetsIndexBuffer, depthtex1Index), sizeof(uint32_t)};
        m_fieldMap["depthtex2Index"]    = {8, offsetof(RenderTargetsIndexBuffer, depthtex2Index), sizeof(uint32_t)};
        m_fieldMap["shadowtex0Index"]   = {8, offsetof(RenderTargetsIndexBuffer, shadowtex0Index), sizeof(uint32_t)};
        m_fieldMap["shadowtex1Index"]   = {8, offsetof(RenderTargetsIndexBuffer, shadowtex1Index), sizeof(uint32_t)};
        m_fieldMap["shadowcolor0Index"] = {8, offsetof(RenderTargetsIndexBuffer, shadowcolor0Index), sizeof(uint32_t)};
        m_fieldMap["shadowcolor1Index"] = {8, offsetof(RenderTargetsIndexBuffer, shadowcolor1Index), sizeof(uint32_t)};
    }

    // ========================================================================
    // CreateGPUBuffers() - 创建9个GPU StructuredBuffer
    // ========================================================================

    void UniformManager::CreateGPUBuffers()
    {
        // Category 0: CameraAndPlayerUniforms
        m_cameraAndPlayerBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(CameraAndPlayerUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"CameraAndPlayerBuffer"
        );

        // Category 1: PlayerStatusUniforms
        m_playerStatusBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(PlayerStatusUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"PlayerStatusBuffer"
        );

        // Category 2: ScreenAndSystemUniforms
        m_screenAndSystemBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(ScreenAndSystemUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"ScreenAndSystemBuffer"
        );

        // Category 3: IDUniforms
        m_idBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(IDUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"IDBuffer"
        );

        // Category 4: WorldAndWeatherUniforms
        m_worldAndWeatherBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(WorldAndWeatherUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"WorldAndWeatherBuffer"
        );

        // Category 5: BiomeAndDimensionUniforms
        m_biomeAndDimensionBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(BiomeAndDimensionUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"BiomeAndDimensionBuffer"
        );

        // Category 6: RenderingUniforms
        m_renderingBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(RenderingUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"RenderingBuffer"
        );

        // Category 7: MatricesUniforms
        m_matricesBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(MatricesUniforms),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"MatricesBuffer"
        );

        // Category 8: RenderTargetsIndexBuffer
        m_renderTargetsIndexBuffer = D12Buffer::CreateStructuredBuffer(
            sizeof(RenderTargetsIndexBuffer),
            1,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            L"RenderTargetsIndexBuffer"
        );
    }

    // ========================================================================
    // RegisterToBindlessSystem() - 注册9个Buffer到Bindless系统
    // ========================================================================

    void UniformManager::RegisterToBindlessSystem()
    {
        BindlessResourceManager* bindlessMgr = BindlessResourceManager::GetInstance();

        // 注册并获取索引
        m_rootConstants.cameraAndPlayerBufferIndex   = bindlessMgr->RegisterBuffer(m_cameraAndPlayerBuffer);
        m_rootConstants.playerStatusBufferIndex      = bindlessMgr->RegisterBuffer(m_playerStatusBuffer);
        m_rootConstants.screenAndSystemBufferIndex   = bindlessMgr->RegisterBuffer(m_screenAndSystemBuffer);
        m_rootConstants.idBufferIndex                = bindlessMgr->RegisterBuffer(m_idBuffer);
        m_rootConstants.worldAndWeatherBufferIndex   = bindlessMgr->RegisterBuffer(m_worldAndWeatherBuffer);
        m_rootConstants.biomeAndDimensionBufferIndex = bindlessMgr->RegisterBuffer(m_biomeAndDimensionBuffer);
        m_rootConstants.renderingBufferIndex         = bindlessMgr->RegisterBuffer(m_renderingBuffer);
        m_rootConstants.matricesBufferIndex          = bindlessMgr->RegisterBuffer(m_matricesBuffer);
        m_rootConstants.renderTargetsBufferIndex     = bindlessMgr->RegisterBuffer(m_renderTargetsIndexBuffer);
    }

    // ========================================================================
    // UnregisterFromBindlessSystem() - 注销9个Buffer
    // ========================================================================

    void UniformManager::UnregisterFromBindlessSystem()
    {
        BindlessResourceManager* bindlessMgr = BindlessResourceManager::GetInstance();

        bindlessMgr->UnregisterBuffer(m_rootConstants.cameraAndPlayerBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.playerStatusBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.screenAndSystemBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.idBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.worldAndWeatherBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.biomeAndDimensionBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.renderingBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.matricesBufferIndex);
        bindlessMgr->UnregisterBuffer(m_rootConstants.renderTargetsBufferIndex);
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
    // RenderTargetsIndexBuffer 专用API
    // ========================================================================

    void UniformManager::UpdateRenderTargetsReadIndices(const uint32_t readIndices[16])
    {
        std::memcpy(m_renderTargetsIndexBuffer.readIndices, readIndices, sizeof(uint32_t) * 16);
        m_renderTargetsDirty = true;
    }

    void UniformManager::UpdateRenderTargetsWriteIndices(const uint32_t writeIndices[16])
    {
        std::memcpy(m_renderTargetsIndexBuffer.writeIndices, writeIndices, sizeof(uint32_t) * 16);
        m_renderTargetsDirty = true;
    }

    void UniformManager::FlipRenderTargets(const uint32_t mainIndices[16],
                                           const uint32_t altIndices[16],
                                           bool           useAlt)
    {
        if (useAlt)
        {
            // 读取Alt, 写入Main
            std::memcpy(m_renderTargetsIndexBuffer.readIndices, altIndices, sizeof(uint32_t) * 16);
            std::memcpy(m_renderTargetsIndexBuffer.writeIndices, mainIndices, sizeof(uint32_t) * 16);
        }
        else
        {
            // 读取Main, 写入Alt
            std::memcpy(m_renderTargetsIndexBuffer.readIndices, mainIndices, sizeof(uint32_t) * 16);
            std::memcpy(m_renderTargetsIndexBuffer.writeIndices, altIndices, sizeof(uint32_t) * 16);
        }

        m_renderTargetsDirty = true;
    }

    // ========================================================================
    // SyncToGPU() - 执行所有Supplier并上传脏Buffer
    // ========================================================================

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
        case 8: return &m_renderTargetsIndexBuffer;
        default:
            ERROR_AND_DIE("UniformManager::GetCategoryDataPtr - Invalid categoryIndex: " + std::to_string(categoryIndex));
            return nullptr;
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
        default:
            ERROR_AND_DIE("UniformManager::MarkCategoryDirty - Invalid categoryIndex: " + std::to_string(categoryIndex));
        }
    }

    // ========================================================================
    // UploadDirtyBuffersToGPU() - 上传所有脏Buffer到GPU
    // ========================================================================

    bool UniformManager::UploadDirtyBuffersToGPU()
    {
        if (m_cameraAndPlayerDirty)
        {
            m_cameraAndPlayerBuffer->UploadData(&m_cameraAndPlayerUniforms, sizeof(CameraAndPlayerUniforms));
            m_cameraAndPlayerDirty = false;
        }

        if (m_playerStatusDirty)
        {
            m_playerStatusBuffer->UploadData(&m_playerStatusUniforms, sizeof(PlayerStatusUniforms));
            m_playerStatusDirty = false;
        }

        if (m_screenAndSystemDirty)
        {
            m_screenAndSystemBuffer->UploadData(&m_screenAndSystemUniforms, sizeof(ScreenAndSystemUniforms));
            m_screenAndSystemDirty = false;
        }

        if (m_idDirty)
        {
            m_idBuffer->UploadData(&m_idUniforms, sizeof(IDUniforms));
            m_idDirty = false;
        }

        if (m_worldAndWeatherDirty)
        {
            m_worldAndWeatherBuffer->UploadData(&m_worldAndWeatherUniforms, sizeof(WorldAndWeatherUniforms));
            m_worldAndWeatherDirty = false;
        }

        if (m_biomeAndDimensionDirty)
        {
            m_biomeAndDimensionBuffer->UploadData(&m_biomeAndDimensionUniforms, sizeof(BiomeAndDimensionUniforms));
            m_biomeAndDimensionDirty = false;
        }

        if (m_renderingDirty)
        {
            m_renderingBuffer->UploadData(&m_renderingUniforms, sizeof(RenderingUniforms));
            m_renderingDirty = false;
        }

        if (m_matricesDirty)
        {
            m_matricesBuffer->UploadData(&m_matricesUniforms, sizeof(MatricesUniforms));
            m_matricesDirty = false;
        }

        if (m_renderTargetsDirty)
        {
            m_renderTargetsIndexBuffer->UploadData(&m_renderTargetsIndexBuffer, sizeof(RenderTargetsIndexBuffer));
            m_renderTargetsDirty = false;
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

    uint32_t UniformManager::GetRenderTargetsBufferIndex() const
    {
        return m_rootConstants.renderTargetsBufferIndex;
    }

    // ========================================================================
    // Reset() - 重置为默认值
    // ========================================================================

    void UniformManager::Reset()
    {
        // 重置所有9个CPU端结构体为默认值
        m_cameraAndPlayerUniforms   = CameraAndPlayerUniforms();
        m_playerStatusUniforms      = PlayerStatusUniforms();
        m_screenAndSystemUniforms   = ScreenAndSystemUniforms();
        m_idUniforms                = IDUniforms();
        m_worldAndWeatherUniforms   = WorldAndWeatherUniforms();
        m_biomeAndDimensionUniforms = BiomeAndDimensionUniforms();
        m_renderingUniforms         = RenderingUniforms();
        m_matricesUniforms          = MatricesUniforms();
        m_renderTargetsIndexBuffer  = RenderTargetsIndexBuffer();

        // 标记所有为脏
        m_cameraAndPlayerDirty   = true;
        m_playerStatusDirty      = true;
        m_screenAndSystemDirty   = true;
        m_idDirty                = true;
        m_worldAndWeatherDirty   = true;
        m_biomeAndDimensionDirty = true;
        m_renderingDirty         = true;
        m_matricesDirty          = true;
        m_renderTargetsDirty     = true;

        // 清空Supplier列表
        m_uniformGetters.clear();
    }
} // namespace enigma::graphic
