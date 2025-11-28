#pragma once
#include <string>

namespace enigma::registry::block
{
    /**
     * @brief NeoForge-style Block Properties configuration
     *
     * This class holds all the configuration options for creating a Block instance,
     * following the NeoForge pattern where blocks are constructed with a Properties object.
     */
    class BlockProperties
    {
    private:
        float       m_hardness           = 1.0f;
        float       m_resistance         = 1.0f;
        bool        m_isOpaque           = true;
        bool        m_isFullBlock        = true;
        bool        m_requiresCorrectTool = false;
        std::string m_blockstatePath;
        std::string m_modelPath;
        std::string m_textureName;       // For simple blocks with single texture
        bool        m_hasDefaultTexture  = false;

    public:
        BlockProperties() = default;

        /**
         * @brief Set block hardness (affects mining time)
         */
        BlockProperties& hardness(float value)
        {
            m_hardness = value;
            return *this;
        }

        /**
         * @brief Set explosion resistance
         */
        BlockProperties& resistance(float value)
        {
            m_resistance = value;
            return *this;
        }

        /**
         * @brief Set if block is opaque (affects lighting)
         */
        BlockProperties& opaque(bool value)
        {
            m_isOpaque = value;
            return *this;
        }

        /**
         * @brief Set if block is a full cube (affects collision)
         */
        BlockProperties& fullBlock(bool value)
        {
            m_isFullBlock = value;
            return *this;
        }

        /**
         * @brief Set if block requires correct tool to harvest
         */
        BlockProperties& requiresCorrectTool(bool value)
        {
            m_requiresCorrectTool = value;
            return *this;
        }

        /**
         * @brief Set custom blockstate definition path
         */
        BlockProperties& blockstatePath(const std::string& path)
        {
            m_blockstatePath = path;
            return *this;
        }

        /**
         * @brief Set custom model path
         */
        BlockProperties& modelPath(const std::string& path)
        {
            m_modelPath = path;
            return *this;
        }

        /**
         * @brief Set texture name for simple single-texture blocks
         */
        BlockProperties& texture(const std::string& textureName)
        {
            m_textureName = textureName;
            return *this;
        }

        /**
         * @brief Enable default texture generation (for blocks without assets)
         */
        BlockProperties& useDefaultTexture()
        {
            m_hasDefaultTexture = true;
            return *this;
        }

        // Static factory methods for common configurations
        static BlockProperties solid()
        {
            return BlockProperties().hardness(1.0f).resistance(1.0f).opaque(true).fullBlock(true);
        }

        static BlockProperties transparent()
        {
            return BlockProperties().hardness(1.0f).resistance(1.0f).opaque(false).fullBlock(false);
        }

        static BlockProperties air()
        {
            return BlockProperties()
                .hardness(0.0f)
                .resistance(0.0f)
                .opaque(false)
                .fullBlock(false)
                .useDefaultTexture();
        }

        static BlockProperties stone()
        {
            return BlockProperties().hardness(1.5f).resistance(6.0f).requiresCorrectTool(true);
        }

        static BlockProperties wood()
        {
            return BlockProperties().hardness(2.0f).resistance(3.0f);
        }

        // Getters for Block construction
        float getHardness() const { return m_hardness; }
        float getResistance() const { return m_resistance; }
        bool isOpaque() const { return m_isOpaque; }
        bool isFullBlock() const { return m_isFullBlock; }
        bool requiresCorrectTool() const { return m_requiresCorrectTool; }
        const std::string& getBlockstatePath() const { return m_blockstatePath; }
        const std::string& getModelPath() const { return m_modelPath; }
        const std::string& getTextureName() const { return m_textureName; }
        bool hasDefaultTexture() const { return m_hasDefaultTexture; }
    };
}