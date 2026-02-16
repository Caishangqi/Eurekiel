/**
 * @file worldinfo_uniforms.hlsl
 * @brief World Info Uniforms Constant Buffer - Iris WorldInfoUniforms.java Compatible
 * @date 2026-02-14
 * @version v1.0
 *
 * [Iris Reference]
 *   Source: WorldInfoUniforms.java
 *   Path: Iris-multiloader-new/common/src/main/java/net/irisshaders/iris/uniforms/WorldInfoUniforms.java
 *
 * [Iris Fields]
 *   cloudHeight          - Runtime cloud height (default 192.0)
 *   heightLimit          - Dimension total height (default 384)
 *   bedrockLevel         - Dimension min Y (default 0)
 *   logicalHeightLimit   - Logical height limit (default 384)
 *   ambientLight         - Ambient light level (default 0.0)
 *   hasCeiling           - Has ceiling (nether=1, default 0)
 *   hasSkylight          - Has skylight (default 1)
 *
 * [Memory Layout] 32 bytes (2 rows x 16 bytes)
 *   Row 0 [0-15]:  cloudHeight, heightLimit, bedrockLevel, logicalHeightLimit
 *   Row 1 [16-31]: ambientLight, hasCeiling, hasSkylight, _pad1
 *
 * C++ Counterpart: Code/Game/Framework/RenderPass/ConstantBuffer/WorldInfoUniforms.hpp
 */

cbuffer WorldInfoUniforms : register(b3, space1)
{
    // ==================== Row 0: Cloud & Height (16 bytes) ====================
    // @iris cloudHeight — runtime cloud height (CR uses as offset base)
    float cloudHeight;

    // @iris heightLimit — dimension total height
    int heightLimit;

    // @iris bedrockLevel — dimension min Y
    int bedrockLevel;

    // @iris logicalHeightLimit — logical height limit
    int logicalHeightLimit;

    // ==================== Row 1: Dimension Properties (16 bytes) ====================
    // @iris ambientLight — ambient light level
    float ambientLight;

    // @iris hasCeiling — has ceiling (nether=1)
    int hasCeiling;

    // @iris hasSkylight — has skylight
    int hasSkylight;

    // Padding
    int WorldInfoUniforms_pad1;
};
