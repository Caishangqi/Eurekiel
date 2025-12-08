#pragma once

#include "CameraCreateInfo.hpp"
#include "Engine/Graphic/Shader/Uniform/MatricesUniforms.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/EulerAngles.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

namespace enigma::graphic
{
    /**
     * @brief EnigmaCamera - DX12延迟渲染引擎专用相机类
     *
     * 设计原则：
     * 1. 纯粹数据载体，不包含任何GPU操作
     * 2. 不继承任何类，独立设计
     * 3. 提供Getters供RendererSubsystem读取数据
     * 4. 支持透视和正交两种投影模式
     *
     * 数据流：
     * EnigmaCamera(数据载体) → RendererSubsystem::BeginCamera(控制逻辑) → UniformManager(数据管理) → GPU
     *
     * 使用示例：
     * ```cpp
     * // 创建世界相机（透视投影）
     * CameraCreateInfo worldInfo = CameraCreateInfo::CreatePerspective(
     *     Vec3(0, 2, 10), EulerAngles::ZERO, 16.0f/9.0f, 60.0f);
     * EnigmaCamera worldCam(worldInfo);
     *
     * // 创建屏幕相机（正交投影）
     * CameraCreateInfo screenInfo = CameraCreateInfo::CreateUI2D(Vec2(1920, 1080));
     * EnigmaCamera screenCam(screenInfo);
     *
     * // 在渲染循环中使用
     * renderer->BeginCamera(worldCam);
     * // ... 渲染场景 ...
     * renderer->BeginCamera(screenCam);
     * // ... 渲染UI ...
     * ```
     */
    class EnigmaCamera
    {
    public:
        // ========================================================================
        // 构造函数
        // ========================================================================

        /**
         * @brief 构造函数 - 接收CameraCreateInfo配置
         * @param createInfo 相机创建信息
         *
         * 教学要点：
         * 1. 构造时只保存配置，不执行任何GPU操作
         * 2. 纯粹数据载体，符合RAII原则
         * 3. 自动计算相关矩阵
         */
        explicit EnigmaCamera(const CameraCreateInfo& createInfo);

        /**
         * @brief 默认构造函数 - 创建标准透视相机
         *
         * 教学要点：
         * 1. 提供合理的默认配置
         * 2. 适合快速原型和测试
         */
        EnigmaCamera();

        // ========================================================================
        // Getters - 供RendererSubsystem::BeginCamera使用
        // ========================================================================

        /**
         * @brief 获取相机位置
         * @return 世界空间中的相机位置
         */
        const Vec3& GetPosition() const { return m_createInfo.position; }

        /**
         * @brief 获取相机朝向
         * @return 欧拉角表示的相机朝向
         */
        const EulerAngles& GetOrientation() const { return m_createInfo.orientation; }

        /**
         * @brief 获取相机模式
         * @return 透视或正交投影模式
         */
        CameraMode GetMode() const { return m_createInfo.mode; }

        /**
         * @brief 获取视口
         * @return 标准化视口坐标(0-1)
         */
        const AABB2& GetViewport() const { return m_createInfo.viewport; }

        /**
         * @brief 获取相机到渲染空间的变换矩阵
         * @return 游戏坐标系到DirectX坐标系的转换矩阵
         */
        const Mat44& GetCameraToRenderTransform() const { return m_createInfo.cameraToRenderTransform; }

        // ========================================================================
        // 矩阵计算 - 核心数学功能
        // ========================================================================

        /**
         * @brief 获取世界空间到相机空间的变换矩阵
         * @return 世界空间到相机空间的变换矩阵
         *
         * 教学要点：
         * 1. 这是GetCameraToWorldTransform()的逆矩阵
         * 2. 用于将世界坐标转换到相机坐标
         * 3. 基于位置和朝向动态计算
         */
        Mat44 GetWorldToCameraTransform() const;

        /**
         * @brief 获取相机空间到世界空间的变换矩阵
         * @return 相机空间到世界空间的变换矩阵
         *
         * 教学要点：
         * 1. 基于位置和朝向计算
         * 2. 用于将相机坐标转换到世界坐标
         * 3. GetWorldToCameraTransform()的逆矩阵
         */
        Mat44 GetCameraToWorldTransform() const;

        /**
         * @brief 获取投影矩阵
         * @return 根据模式返回透视或正交投影矩阵
         *
         * 教学要点：
         * 1. 透视模式：使用FOV和宽高比
         * 2. 正交模式：使用视锥体边界
         * 3. 包含近远平面裁剪
         */
        Mat44 GetProjectionMatrix() const;

        /**
         * @brief 获取完整的视图投影矩阵
         * @return 世界空间到裁剪空间的变换矩阵
         *
         * 教学要点：
         * 1. View * Projection的组合
         * 2. 常用于直接变换世界坐标到裁剪坐标
         * 3. 性能优化的关键点
         */
        Mat44 GetViewProjectionMatrix() const;

        // ========================================================================
        // 投影参数访问器
        // ========================================================================

        /**
         * @brief 获取透视投影参数
         */
        float GetPerspectiveAspect() const { return m_createInfo.perspectiveAspect; }
        float GetPerspectiveFOV() const { return m_createInfo.perspectiveFOV; }
        float GetPerspectiveNear() const { return m_createInfo.perspectiveNear; }
        float GetPerspectiveFar() const { return m_createInfo.perspectiveFar; }

        /**
         * @brief 获取正交投影参数
         */
        const Vec2&      GetOrthographicBottomLeft() const { return m_createInfo.orthographicBottomLeft; }
        const Vec2&      GetOrthographicTopRight() const { return m_createInfo.orthographicTopRight; }
        float            GetOrthographicNear() const { return m_createInfo.orthographicNear; }
        float            GetOrthographicFar() const { return m_createInfo.orthographicFar; }
        MatricesUniforms GetMatricesUniforms() const;

        // ========================================================================
        // 配置更新
        // ========================================================================

        /**
         * @brief 更新创建信息
         * @param createInfo 新的创建信息
         *
         * 教学要点：
         * 1. 动态修改相机配置
         * 2. 自动标记需要重新计算矩阵
         * 3. 保持数据一致性
         */
        void UpdateCreateInfo(const CameraCreateInfo& createInfo);

        /**
         * @brief 设置位置和朝向
         * @param position 新的相机位置
         * @param orientation 新的相机朝向
         */
        void SetPositionAndOrientation(const Vec3& position, const EulerAngles& orientation);

        /**
         * @brief 设置位置
         * @param position 新的相机位置
         */
        void SetPosition(const Vec3& position);

        /**
         * @brief 设置朝向
         * @param orientation 新的相机朝向
         */
        void SetOrientation(const EulerAngles& orientation);

        // ========================================================================
        // 实用功能
        // ========================================================================

        /**
         * @brief 获取世界空间向前向量
         * @return 前向向量（normalized）
         */
        Vec3 GetForwardVector() const;

        /**
         * @brief 获取世界空间向右向量
         * @return 右向量（normalized）
         */
        Vec3 GetRightVector() const;

        /**
         * @brief 获取世界空间向上向量
         * @return 上向量（normalized）
         */
        Vec3 GetUpVector() const;

        /**
         * @brief 将世界坐标转换为屏幕坐标
         * @param worldPos 世界空间位置
         * @param clientSize 客户区大小
         * @return 屏幕空间位置
         */
        Vec2 WorldToScreen(const Vec3& worldPos, const Vec2& clientSize) const;

        /**
         * @brief 检查是否为透视相机
         * @return 如果是透视投影返回true
         */
        bool IsPerspective() const { return m_createInfo.mode == CameraMode::Perspective; }

        /**
         * @brief 检查是否为正交相机
         * @return 如果是正交投影返回true
         */
        bool IsOrthographic() const { return m_createInfo.mode == CameraMode::Orthographic; }

    private:
        // ========================================================================
        // 成员变量
        // ========================================================================

        CameraCreateInfo m_createInfo; // 创建配置信息

        // 禁用拷贝构造和赋值（遵循RAII原则）
        EnigmaCamera(const EnigmaCamera&)            = delete;
        EnigmaCamera& operator=(const EnigmaCamera&) = delete;
    };
} // namespace enigma::graphic
