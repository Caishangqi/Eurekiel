#pragma once
#include "Engine/Resource/Model/ModelResource.hpp"

namespace enigma::renderer::model
{
    class ModelBuiltin
    {
    public:
        [[maybe_unused]] static resource::model::ModelResourcePtr CreateBlockCube();
        [[maybe_unused]] static resource::model::ModelResourcePtr CreateBlockCubeAll();
    };
}
