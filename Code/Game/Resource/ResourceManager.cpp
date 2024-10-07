#include "ResourceManager.hpp"

#include "SoundRes.hpp"
#include "Game/GameCommon.hpp"

ResourceManager::ResourceManager()
{
}

ResourceManager::~ResourceManager()
{
}

void ResourceManager::Startup()
{
    printf("[resource]    Loading resources...\n");
    ResisterSound();
}

void ResourceManager::ResisterSound()
{
    SOUND::PLAYER_SHOOTS_BULLET = g_theAudio->CreateOrGetSound(PLAYER_SHOOTS_BULLET_LOCATION);
}

