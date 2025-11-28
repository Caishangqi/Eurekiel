#pragma once

// 基于 Iris 源码: net.irisshaders.iris.uniforms.CameraUniforms (引用)
// 文档验证: 2025-09-14 通过CommonUniforms.java第117行确认存在
// 
// 教学要点:
// 1. 在Iris中通过 CameraUniforms.addCameraUniforms() 调用
// 2. 负责管理相机相关的uniform变量
// 3. 包含相机位置、方向、投影矩阵相关的uniform

#include <memory>

namespace enigma::graphic
{
    /**
     * @brief 相机Uniform管理器
     * 
     * 对应 Iris 中通过 CameraUniforms.addCameraUniforms() 调用的相机uniform系统
     * 负责管理所有与相机相关的着色器uniform变量，包括：
     * - 相机世界位置和相对位置
     * - 相机方向向量
     * - 投影相关参数
     * - 视角和纵横比
     * 
     * TODO: 基于 Iris 实现分析完整的相机uniform逻辑
     * - 实现 addCameraUniforms() 方法 (对应 Iris 的同名方法)
     * - 支持相机变换矩阵uniform
     * - 支持第一人称和第三人称视角切换
     * - 集成到 UniformHolder 系统中
     */
    class CameraUniforms final
    {
    private:
        // 禁止实例化 - 静态工具类
        CameraUniforms()                                 = delete;
        ~CameraUniforms()                                = delete;
        CameraUniforms(const CameraUniforms&)            = delete;
        CameraUniforms& operator=(const CameraUniforms&) = delete;

    public:
        /**
         * @brief 添加相机相关uniform (对应Iris的addCameraUniforms)
         * @param uniforms Uniform持有器
         * @param updateNotifier 帧更新通知器
         * 
         * TODO: 实现相机uniform注册，包括：
         * - cameraPosition (相机世界位置)
         * - previousCameraPosition (上一帧相机位置，用于运动模糊)
         * - cameraDirection (相机朝向)
         * - upVector (相机上方向)
         * - rightVector (相机右方向) 
         * - viewWidth, viewHeight (视口尺寸)
         * - aspectRatio (纵横比)
         * - fov (视野角度)
         * - near, far (近远裁剪面)
         */
        static void AddCameraUniforms(/* UniformHolder& uniforms,
                                       FrameUpdateNotifier& updateNotifier */);

        // ========================================================================
        // 相机状态获取方法
        // ========================================================================

        /**
         * @brief 获取当前相机世界位置
         * @return 相机在世界坐标系中的位置
         * 
         * TODO: 实现相机位置获取，支持：
         * - 第一人称视角 (玩家眼部位置)
         * - 第三人称视角 (相机实际位置)
         * - 观察者模式相机
         */
        // static Vector3d GetCameraWorldPosition();

        /**
         * @brief 获取相机相对位置 (用于着色器)
         * @return 相机相对于某个参考点的位置
         * 
         * TODO: 实现相对位置计算，用于：
         * - 减少精度损失 (大世界坐标)
         * - 着色器中的位置计算
         */
        // static Vector3f GetCameraRelativePosition();

        /**
         * @brief 获取相机朝向向量
         * @return 标准化的朝向向量
         * 
         * TODO: 实现朝向计算，基于：
         * - 玩家的俯仰角和偏航角
         * - 第三人称相机的实际朝向
         */
        // static Vector3f GetCameraDirection();

        /**
         * @brief 获取相机上方向向量
         * @return 标准化的上方向向量
         */
        // static Vector3f GetCameraUpVector();

        /**
         * @brief 获取相机右方向向量
         * @return 标准化的右方向向量
         */
        // static Vector3f GetCameraRightVector();

        /**
         * @brief 获取视野角度 (FOV)
         * @return FOV值 (弧度或角度)
         * 
         * TODO: 实现FOV获取，考虑：
         * - 用户设置的FOV
         * - 动态FOV效果 (冲刺、药水效果等)
         * - 缩放效果 (望远镜等)
         */
        static float GetFieldOfView();

        /**
         * @brief 获取纵横比
         * @return 视口宽度/高度比
         */
        static float GetAspectRatio();

        /**
         * @brief 获取近裁剪面距离
         * @return 近裁剪面距离
         */
        static float GetNearPlane();

        /**
         * @brief 获取远裁剪面距离
         * @return 远裁剪面距离
         * 
         * TODO: 实现动态远裁剪面，基于：
         * - 渲染距离设置
         * - 性能优化需求
         * - 特效需求 (如无限距离天空盒)
         */
        static float GetFarPlane();

        /**
         * @brief 检查是否为第一人称视角
         * @return 第一人称返回true，否则返回false
         */
        static bool IsFirstPersonView();

        /**
         * @brief 获取视口尺寸
         * @return 视口的宽度和高度 (像素)
         */
        // static Vector2i GetViewportSize();
    };
} // namespace enigma::graphic
