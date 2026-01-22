#pragma once
#include "../Property/PropertyMap.hpp"
#include <memory>
#include <unordered_map>
#include <vector>

namespace enigma::voxel
{
    /**
     * @brief Generic state holder base class for Block and Fluid states
     *
     * [MINECRAFT REF] StateHolder.java
     * File: net/minecraft/world/level/block/state/StateHolder.java
     *
     * This template class provides the common state management functionality
     * shared between BlockState and FluidState:
     * - Owner reference (Block or Fluid)
     * - Property values storage
     * - State switching via setValue()
     * - Neighbour state lookup table for fast state transitions
     *
     * Template Parameters:
     * - O: Owner type (Block for BlockState, Fluid for FluidState)
     * - S: Self type (BlockState or FluidState) for CRTP pattern
     *
     * [DEVIATION] Simplified version:
     * - No MapCodec serialization (handled separately)
     * - Uses PropertyMap instead of Reference2ObjectArrayMap
     * - Neighbour table is optional (populated on demand)
     *
     * Usage:
     * ```cpp
     * class BlockState : public StateHolder<Block, BlockState> { ... };
     * class FluidState : public StateHolder<Fluid, FluidState> { ... };
     * ```
     */
    template <typename O, typename S>
    class StateHolder
    {
    public:
        using OwnerType = O;
        using StateType = S;

    protected:
        // ============================================================
        // Core Members
        // [MINECRAFT REF] StateHolder.java:35-38
        // ============================================================

        /**
         * @brief The owner object (Block or Fluid)
         * [MINECRAFT REF] protected final O owner;
         */
        O* m_owner = nullptr;

        /**
         * @brief Property values for this state
         * [MINECRAFT REF] private final ImmutableMap<Property<?>, Comparable<?>> values;
         */
        PropertyMap m_values;

        /**
         * @brief Neighbour state lookup table for fast state transitions
         * [MINECRAFT REF] private Table<Property<?>, Comparable<?>, S> neighbours;
         *
         * Key: Property pointer -> Value -> Resulting state
         * Populated lazily via PopulateNeighbours()
         */
        mutable std::unordered_map<IProperty*, std::unordered_map<size_t, S*>> m_neighbours;
        mutable bool                                                           m_neighboursPopulated = false;

    public:
        // ============================================================
        // Constructors
        // ============================================================

        StateHolder() = default;

        /**
         * @brief Construct with owner and property values
         * [MINECRAFT REF] StateHolder constructor
         */
        StateHolder(O* owner, const PropertyMap& values)
            : m_owner(owner)
              , m_values(values)
        {
        }

        virtual ~StateHolder() = default;

        // ============================================================
        // Owner Access
        // ============================================================

        /**
         * @brief Get the owner object
         * [MINECRAFT REF] No direct equivalent, but owner is accessed via getBlock()/getType()
         */
        O* GetOwner() const { return m_owner; }

        // ============================================================
        // Property Value Access
        // [MINECRAFT REF] StateHolder.java:67-75 getValue()
        // ============================================================

        /**
         * @brief Get a property value (type-safe)
         *
         * [MINECRAFT REF] public <T extends Comparable<T>> T getValue(Property<T> property)
         *
         * @tparam T Property value type
         * @param property The property to query
         * @return The current value of the property
         */
        template <typename T>
        T GetValue(std::shared_ptr<Property<T>> property) const
        {
            return m_values.Get(property);
        }

        /**
         * @brief Get property values map (read-only)
         */
        const PropertyMap& GetValues() const { return m_values; }

        // ============================================================
        // State Switching
        // [MINECRAFT REF] StateHolder.java:89-108 setValue()
        // ============================================================

        /**
         * @brief Get a state with one property changed
         *
         * [MINECRAFT REF] public <T extends Comparable<T>, V extends T> S setValue(Property<T> property, V value)
         *
         * This method returns a different state object with the specified property
         * changed to the new value. The current state is not modified (immutable pattern).
         *
         * @tparam T Property value type
         * @param property The property to change
         * @param value The new value
         * @return Pointer to the state with the new value, or nullptr if invalid
         */
        template <typename T>
        S* SetValue(std::shared_ptr<Property<T>> property, const T& value) const
        {
            // Validate property and value
            if (!property || !property->IsValidValue(value))
            {
                return nullptr;
            }

            // Check if value is already set (return self)
            if (m_values.Get(property) == value)
            {
                return const_cast<S*>(static_cast<const S*>(this));
            }

            // Look up in neighbours table if populated
            if (m_neighboursPopulated)
            {
                auto propIt = m_neighbours.find(property.get());
                if (propIt != m_neighbours.end())
                {
                    size_t valueHash = std::hash<T>{}(value);
                    auto   valueIt   = propIt->second.find(valueHash);
                    if (valueIt != propIt->second.end())
                    {
                        return valueIt->second;
                    }
                }
            }

            // Fallback: search through owner's states (implemented by derived class)
            return FindStateWithValue(property, value);
        }

        /**
         * @brief Cycle to the next value of a property
         *
         * [MINECRAFT REF] public <T extends Comparable<T>> S cycle(Property<T> property)
         *
         * @tparam T Property value type
         * @param property The property to cycle
         * @return Pointer to the state with the next value
         */
        template <typename T>
        S* Cycle(std::shared_ptr<Property<T>> property) const
        {
            if (!property)
            {
                return const_cast<S*>(static_cast<const S*>(this));
            }

            const auto& possibleValues = property->GetPossibleValues();
            if (possibleValues.empty())
            {
                return const_cast<S*>(static_cast<const S*>(this));
            }

            T currentValue = m_values.Get(property);

            // Find current index and get next
            for (size_t i = 0; i < possibleValues.size(); ++i)
            {
                if (possibleValues[i] == currentValue)
                {
                    size_t nextIndex = (i + 1) % possibleValues.size();
                    return SetValue(property, possibleValues[nextIndex]);
                }
            }

            return const_cast<S*>(static_cast<const S*>(this));
        }

        // ============================================================
        // Neighbour Table Management
        // [MINECRAFT REF] StateHolder.java:110-130 populateNeighbours()
        // ============================================================

        /**
         * @brief Populate the neighbour state lookup table
         *
         * [MINECRAFT REF] public void populateNeighbours(Map<Map<Property<?>, Comparable<?>>, S> states)
         *
         * This builds a fast lookup table for state transitions.
         * Called once after all states are created.
         *
         * @param allStates Map of all possible states for this owner
         */
        void PopulateNeighbours(const std::vector<S*>& allStates)
        {
            if (m_neighboursPopulated)
            {
                return;
            }

            m_neighbours.clear();

            // For each property in this state
            for (const auto& prop : m_values.GetProperties())
            {
                auto& valueMap = m_neighbours[prop.get()];

                // For each possible value of this property
                for (const auto& valueStr : prop->GetPossibleValuesAsStrings())
                {
                    std::any value     = prop->StringToValue(valueStr);
                    size_t   valueHash = prop->GetValueHash(value);

                    // Find the state with this property value
                    PropertyMap targetValues = m_values;
                    targetValues.SetAny(prop, value);

                    for (S* state : allStates)
                    {
                        if (state->GetValues() == targetValues)
                        {
                            valueMap[valueHash] = state;
                            break;
                        }
                    }
                }
            }

            m_neighboursPopulated = true;
        }

        /**
         * @brief Check if neighbours table is populated
         */
        bool AreNeighboursPopulated() const { return m_neighboursPopulated; }

        // ============================================================
        // Comparison and Hashing
        // ============================================================

        /**
         * @brief Check equality with another state
         */
        bool operator==(const StateHolder& other) const
        {
            return m_owner == other.m_owner && m_values == other.m_values;
        }

        bool operator!=(const StateHolder& other) const
        {
            return !(*this == other);
        }

        /**
         * @brief Get hash for fast lookup
         */
        size_t GetHash() const
        {
            size_t hash = std::hash<void*>{}(m_owner);
            hash        ^= m_values.GetHash() + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            return hash;
        }

        // ============================================================
        // Utility
        // ============================================================

        /**
         * @brief Convert to string representation
         */
        std::string ToString() const
        {
            return m_values.ToString();
        }

    protected:
        // ============================================================
        // Virtual Methods for Derived Classes
        // ============================================================

        /**
         * @brief Find a state with a specific property value
         *
         * Derived classes should implement this to search through
         * the owner's state collection.
         *
         * @tparam T Property value type
         * @param property The property to match
         * @param value The value to match
         * @return Pointer to matching state, or nullptr if not found
         */
        template <typename T>
        S* FindStateWithValue(std::shared_ptr<Property<T>> property, const T& value) const
        {
            // Default implementation returns nullptr
            // Derived classes should override via owner's state collection
            return nullptr;
        }
    };
}
