#include "ModelBuiltin.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Model/ModelSubsystem.hpp"

using namespace enigma::resource::model;

using namespace enigma::renderer::model;

ModelResourcePtr ModelBuiltin::CreateBlockCube()
{
    // Create a complete builtin block/cube model resource
    ModelResourcePtr model = std::make_shared<ModelResource>(ResourceLocation("block/cube"));

    // Define standard cube textures (these will be overridden by child models)
    model->SetTexture("particle", ResourceLocation("missingno"));
    model->SetTexture("down", ResourceLocation("missingno"));
    model->SetTexture("up", ResourceLocation("missingno"));
    model->SetTexture("north", ResourceLocation("missingno"));
    model->SetTexture("south", ResourceLocation("missingno"));
    model->SetTexture("east", ResourceLocation("missingno"));
    model->SetTexture("west", ResourceLocation("missingno"));

    // Create the standard 1x1x1 cube element (0,0,0 to 16,16,16 in Minecraft coordinates)
    ModelElement cubeElement;
    cubeElement.from  = Vec3(0, 0, 0);
    cubeElement.to    = Vec3(16, 16, 16);
    cubeElement.shade = true;

    // Define all 6 faces of the cube
    const std::vector<std::string> faceNames = {"down", "up", "north", "south", "west", "east"};

    for (const auto& faceName : faceNames)
    {
        ModelFace face;
        face.texture                = "#" + faceName; // Reference to texture variable (e.g., "#down")
        face.uv                     = Vec4(0, 0, 16, 16); // Full texture UV (0,0 to 16,16)
        face.cullFace               = faceName; // Cull against same direction
        cubeElement.faces[faceName] = face;
    }

    // Add the cube element to the model
    model->AddElement(cubeElement);

    // Mark as loaded
    model->GetMutableMetadata().state = ResourceState::LOADED;

    LogInfo(LogModel, "Created builtin model: block/cube with 1 element and 6 faces");
    return model;
}


ModelResourcePtr ModelBuiltin::CreateBlockCubeAll()
{
    // Create block/cube_all model (uses same texture for all faces)
    ModelResourcePtr model = std::make_shared<ModelResource>(ResourceLocation("block/cube_all"));

    // Inherit from block/cube
    model->SetParent(ResourceLocation("block/cube"));

    // All faces use the same texture (defined by #all variable)
    model->SetTexture("particle", ResourceLocation("#all"));
    model->SetTexture("down", ResourceLocation("#all"));
    model->SetTexture("up", ResourceLocation("#all"));
    model->SetTexture("north", ResourceLocation("#all"));
    model->SetTexture("south", ResourceLocation("#all"));
    model->SetTexture("east", ResourceLocation("#all"));
    model->SetTexture("west", ResourceLocation("#all"));

    // Mark as loaded
    model->GetMutableMetadata().state = ResourceState::LOADED;

    LogInfo(LogModel, "Created builtin model: block/cube_all");
    return model;
}
