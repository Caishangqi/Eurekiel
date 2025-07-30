#include "ResourceLoader.hpp"

#include <algorithm>

#include "Engine/Resource/ResourceCommon.hpp"

using namespace enigma::resource;

void LoaderRegistry::RegisterLoader(ResourceLoaderPtr loader)
{
    if (!loader) return;

    m_loadersByName[loader->GetLoaderName()] = loader; // Register by name
}
