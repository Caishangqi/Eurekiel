#pragma once
#include "BlockStateDefinition.hpp"
#include "../../Voxel/Property/Property.hpp"
#include "../../Voxel/Property/PropertyMap.hpp"
#include "../ResourceCommon.hpp"
#include "../../Core/Engine.hpp"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace enigma::model
{
    class ModelSubsystem;
}

namespace enigma::registry::block
{
    class Block;
}

namespace enigma::resource::blockstate
{
    using namespace enigma::resource;
    using namespace enigma::voxel;

    /**
     * @brief Builder pattern for creating BlockStateDefinition objects programmatically
     * 
     * Similar to Minecraft NeoForge's BlockStateBuilder, this provides a fluent API
     * for defining blockstate variants and multipart models based on block properties.
     */
    class BlockStateBuilder
    {
    public:
        /**
         * @brief Condition builder for multipart blockstates
         */
        class ConditionBuilder
        {
        private:
            std::map<std::string, std::string> m_conditions;

        public:
            ConditionBuilder() = default;

            /**
             * @brief Add a property condition
             */
            ConditionBuilder& Property(const std::string& name, const std::string& value);

            /**
             * @brief Add a boolean property condition
             */
            ConditionBuilder& Property(const std::string& name, bool value);

            /**
             * @brief Add an integer property condition
             */
            ConditionBuilder& Property(const std::string& name, int value);

            /**
             * @brief Add property condition using typed property
             * Note: Temporarily disabled due to template compilation issues
             */
            // template<typename T>
            // ConditionBuilder& Property(std::shared_ptr<Property<T>> property, const T& value);

            std::map<std::string, std::string> Build() const;
        };

        /**
         * @brief Variant builder for defining model variants
         */
        class VariantBuilder
        {
        private:
            BlockStateVariant m_variant;

        public:
            explicit VariantBuilder(const std::string& modelPath);
            explicit VariantBuilder(const ResourceLocation& modelPath);

            /**
             * @brief Set X rotation (0, 90, 180, 270)
             */
            VariantBuilder& RotationX(int degrees);

            /**
             * @brief Set Y rotation (0, 90, 180, 270)  
             */
            VariantBuilder& RotationY(int degrees);

            /**
             * @brief Set UV lock
             */
            VariantBuilder& UVLock(bool uvlock = true);

            /**
             * @brief Set weight for random selection
             */
            VariantBuilder& Weight(int weight);

            BlockStateVariant Build() const;
        };

        /**
         * @brief Multipart case builder
         */
        class MultipartCaseBuilder
        {
        private:
            std::vector<MultipartCondition> m_conditions;
            std::vector<BlockStateVariant>  m_variants;

        public:
            MultipartCaseBuilder() = default;

            /**
             * @brief Add a condition (OR logic with other conditions)
             */
            MultipartCaseBuilder& When(const ConditionBuilder& condition);

            /**
             * @brief Add a condition using lambda
             */
            MultipartCaseBuilder& When(std::function<ConditionBuilder()> conditionFunc);

            /**
             * @brief Apply a variant when conditions match
             */
            MultipartCaseBuilder& Apply(const VariantBuilder& variant);

            /**
             * @brief Apply a variant using lambda
             */
            MultipartCaseBuilder& Apply(std::function<VariantBuilder()> variantFunc);

            MultipartCase Build() const;
        };

    private:
        ResourceLocation m_location;
        bool             m_isMultipart = false;

        // Variants mode data
        std::map<std::string, std::vector<BlockStateVariant>> m_variants;

        // Multipart mode data
        std::vector<MultipartCase> m_multipartCases;

    public:
        explicit BlockStateBuilder(const ResourceLocation& location);
        explicit BlockStateBuilder(const std::string& location);

        /**
         * @brief Add a variant for specific property combination
         */
        BlockStateBuilder& Variant(const std::string& propertyString, const VariantBuilder& variant);

        /**
         * @brief Add a variant for specific property combination using lambda
         */
        BlockStateBuilder& Variant(const std::string& propertyString, std::function<VariantBuilder()> variantFunc);

        /**
         * @brief Add a variant for property map
         */
        BlockStateBuilder& Variant(const PropertyMap& properties, const VariantBuilder& variant);

        /**
         * @brief Add multiple variants for same property combination
         */
        BlockStateBuilder& Variants(const std::string& propertyString, const std::vector<VariantBuilder>& variants);

        /**
         * @brief Add default variant (empty property string)
         */
        BlockStateBuilder& DefaultVariant(const VariantBuilder& variant);

        /**
         * @brief Add default variant using lambda
         */
        BlockStateBuilder& DefaultVariant(std::function<VariantBuilder()> variantFunc);

        /**
         * @brief Switch to multipart mode and add a case
         */
        BlockStateBuilder& Multipart(const MultipartCaseBuilder& caseBuilder);

        /**
         * @brief Add multipart case using lambda
         */
        BlockStateBuilder& Multipart(std::function<MultipartCaseBuilder()> caseFunc);

        /**
         * @brief Generate variants automatically from block properties
         * Creates all possible property combinations and maps them to model variants
         */
        BlockStateBuilder& AutoGenerateVariants(enigma::registry::block::Block* block, const std::string& baseModelPath, std::function<std::string(const PropertyMap&)> modelPathMapper = nullptr);

        /**
         * @brief Build the final BlockStateDefinition
         */
        std::shared_ptr<BlockStateDefinition> Build();

        /**
         * @brief Create a simple single-variant blockstate
         */
        static std::shared_ptr<BlockStateDefinition> Simple(const ResourceLocation& location, const std::string& modelPath);

        /**
         * @brief Create a blockstate with property-based variants
         */
        static std::shared_ptr<BlockStateDefinition> WithVariants(const ResourceLocation& location, const std::map<std::string, std::string>& variantMap);

    private:
        std::string MapPropertiesToString(const PropertyMap& properties) const;
        void        GeneratePropertyCombinations(enigma::registry::block::Block* block, std::vector<PropertyMap>& combinations) const;

        /**
         * @brief Compile a variant's model using ModelSubsystem
         * This triggers the model compilation pipeline for each block variant
         */
        void CompileVariantModel(BlockStateVariant& variant, enigma::model::ModelSubsystem* modelSubsystem) const;
    };
}
