#pragma once
#include "../../Core/Event/Event.hpp"
#include "../../Core/Event/RegisterEvent.hpp"
#include "../Core/Registry.hpp"
#include "Block.hpp"

namespace enigma::registry::block
{
    using namespace enigma::core;
    using namespace enigma::event;

    /**
     * @class BlockRegisterEvent
     * @brief Event fired when block registration phase begins
     * 
     * This event is dispatched by the engine during startup to allow
     * game code to register blocks. Similar to NeoForge's RegisterEvent<Block>.
     * 
     * Usage:
     * @code
     * // In game initialization
     * eventBus.AddListener<BlockRegisterEvent>([](BlockRegisterEvent& event) {
     *     event.Register("stone", std::make_shared<Block>("stone", "minecraft"));
     *     event.Register("dirt", std::make_shared<Block>("dirt", "minecraft"));
     * });
     * @endcode
     */
    class BlockRegisterEvent : public RegisterEvent<Registry<Block>>
    {
    public:
        explicit BlockRegisterEvent(Registry<Block>& registry)
            : RegisterEvent<Registry<Block>>(registry)
        {
        }

        /**
         * @brief Register a block with name only (uses default namespace)
         */
        void Register(const std::string& name, std::shared_ptr<Block> block)
        {
            GetRegistry().Register(name, block);
        }

        /**
         * @brief Register a block with namespace and name
         */
        void Register(const std::string& namespaceName, const std::string& name, std::shared_ptr<Block> block)
        {
            GetRegistry().Register(namespaceName, name, block);
        }

        /**
         * @brief Register a block with RegistrationKey
         */
        void Register(const RegistrationKey& key, std::shared_ptr<Block> block)
        {
            GetRegistry().Register(key, block);
        }
    };
}
