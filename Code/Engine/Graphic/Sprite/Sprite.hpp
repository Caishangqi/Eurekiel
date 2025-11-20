/**
 * @file Sprite.hpp
 * @brief Sprite class - Single sprite image encapsulation
 *
 * Teaching Focus:
 * 1. Basic sprite concept and data encapsulation
 * 2. UV coordinate system and texture sampling
 * 3. Smart pointer management for texture lifetime
 * 4. Factory pattern to simplify object creation
 *
 * Architecture Design:
 * - Holds D12Texture smart pointer, manages texture resources
 * - Stores UV bounds, pivot point, dimensions and other metadata
 * - Provides CreateFromImage static factory method
 * - Supports move semantics, disables copy (RAII principle)
 *
 * Responsibility Boundary:
 * - Sprite: Data encapsulation for a single sprite
 * - SpriteAtlas: Collection management for multiple sprites
 * - D12Texture: Low-level texture resource management
 */

#pragma once

#include "Engine/Graphic/Resource/Texture/D12Texture.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/IntVec2.hpp"
#include <memory>
#include <string>

namespace enigma::graphic
{
    /**
     * @brief Sprite class - Single sprite image encapsulation
     * @details Holds texture reference and UV coordinates, supports all metadata required for sprite rendering
     *
     * Core Features:
     * - Uses shared_ptr to manage texture lifetime
     * - UV coordinates stored normalized (0.0-1.0 range)
     * - Pivot point support (default center point 0.5, 0.5)
     * - Static factory method simplifies creation
     *
     * Usage Example:
     * @code
     *   // Method 1: Create from image file
     *   Sprite sprite = Sprite::CreateFromImage("textures/player.png", "Player");
     *
     *   // Method 2: Create from SpriteAtlas slicing
     *   auto atlas = std::make_shared<SpriteAtlas>(...);
     *   Sprite sprite(atlas->GetTexture(), AABB2(0.0f, 0.0f, 0.5f, 0.5f), "GridSprite_0");
     * @endcode
     */
    class Sprite
    {
    public:
        // ==================== Constructor ====================

        /**
         * @brief Default constructor - Creates an invalid Sprite
         * @details Used for deferred initialization or placeholder
         */
        Sprite() = default;

        /**
         * @brief Full constructor
         * @param texture Texture smart pointer (can be nullptr, indicating invalid Sprite)
         * @param uvBounds UV bounds (normalized coordinates, range 0.0-1.0)
         * @param name Sprite name (for debugging and lookup)
         * @param pivot Pivot point (normalized coordinates, default center 0.5, 0.5)
         * @param dimensions Sprite dimensions (pixel units, used to calculate render size)
         *
         * Teaching Points:
         * - Uses std::move to optimize smart pointer passing
         * - Pivot point affects sprite rotation and scaling center
         * - UV bounds determine the region sampled from texture
         */
        Sprite(
            std::shared_ptr<D12Texture> texture,
            const AABB2&                uvBounds,
            const std::string&          name,
            const Vec2&                 pivot      = Vec2(0.5f, 0.5f),
            const IntVec2&              dimensions = IntVec2(0, 0)
        );

        /**
         * @brief Move constructor
         * @param other Source Sprite to move from
         *
         * Teaching Point: Supports efficient resource transfer, avoids unnecessary copying
         */
        Sprite(Sprite&& other) noexcept = default;

        /**
         * @brief Move assignment operator
         * @param other Source Sprite to move from
         * @return Reference to self
         */
        Sprite& operator=(Sprite&& other) noexcept = default;

        // Disable copy constructor and assignment (RAII principle)
        Sprite(const Sprite&)            = delete;
        Sprite& operator=(const Sprite&) = delete;

        /**
         * @brief Destructor
         * @details Smart pointers automatically manage texture lifetime
         */
        ~Sprite() = default;

        // ==================== Accessor methods ====================

        /**
         * @brief Get texture smart pointer
         * @return Texture shared_ptr (may be nullptr)
         */
        std::shared_ptr<D12Texture> GetTexture() const { return m_texture; }

        /**
         * @brief Get UV bounds
         * @return UV bounds reference (normalized coordinates)
         */
        const AABB2& GetUVBounds() const { return m_uvBounds; }

        /**
         * @brief Get pivot point
         * @return Pivot point reference (normalized coordinates)
         */
        const Vec2& GetPivot() const { return m_pivot; }

        /**
         * @brief Get sprite name
         * @return Name string reference
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief Get sprite dimensions
         * @return Dimensions (pixel units)
         */
        IntVec2 GetDimensions() const { return m_dimensions; }

        /**
         * @brief Check if Sprite is valid
         * @return Returns true if texture pointer is non-null
         *
         * Teaching Point: Provides unified validity check interface
         */
        bool IsValid() const { return m_texture != nullptr; }

        // ==================== Modifier methods ====================

        /**
         * @brief Set texture
         * @param texture New texture smart pointer
         *
         * Teaching Point: Uses std::move to optimize smart pointer passing
         */
        void SetTexture(std::shared_ptr<D12Texture> texture) { m_texture = std::move(texture); }

        /**
         * @brief Set UV bounds
         * @param uvBounds New UV bounds
         */
        void SetUVBounds(const AABB2& uvBounds) { m_uvBounds = uvBounds; }

        /**
         * @brief Set pivot point
         * @param pivot New pivot point
         */
        void SetPivot(const Vec2& pivot) { m_pivot = pivot; }

        /**
         * @brief Set name
         * @param name New name
         */
        void SetName(const std::string& name) { m_name = name; }

        // ==================== Static factory method ====================

        /**
         * @brief Create Sprite from image file
         * @param imagePath Image file path
         * @param name Sprite name (for debugging)
         * @param pivot Pivot point (default center point)
         * @return Created Sprite object
         *
         * Teaching Points:
         * 1. Factory pattern simplifies object creation
         * 2. Uses Image class to load image data
         * 3. Creates D12Texture and uploads to GPU
         * 4. Automatically calculates full UV bounds (0,0,1,1)
         *
         * Usage Example:
         * @code
         *   Sprite playerSprite = Sprite::CreateFromImage("textures/player.png", "Player");
         * @endcode
         */
        static Sprite CreateFromImage(
            const char* imagePath,
            const char* name,
            const Vec2& pivot = Vec2(0.5f, 0.5f)
        );

    private:
        // ==================== Data members ====================
        std::shared_ptr<D12Texture> m_texture; ///< Texture smart pointer
        AABB2                       m_uvBounds; ///< UV bounds (normalized coordinates 0.0-1.0)
        Vec2                        m_pivot; ///< Pivot point (normalized coordinates, default 0.5, 0.5)
        IntVec2                     m_dimensions; ///< Sprite dimensions (pixel units)
        std::string                 m_name; ///< Sprite name (for debugging)
    };
} // namespace enigma::graphic
