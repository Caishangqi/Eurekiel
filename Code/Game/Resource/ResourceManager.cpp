#include "ResourceManager.hpp"
#include "SoundRes.hpp"
#include "Engine/Audio/AudioSystem.hpp"
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
    SOUND::PLAYER_DIES          = g_theAudio->CreateOrGetSound(PLAYER_DIES_LOCATION);
    SOUND::PLAYER_DAMAGED       = g_theAudio->CreateOrGetSound(PLAYER_DAMAGED_LOCATION);
    SOUND::PLAYER_RESPAWN       = g_theAudio->CreateOrGetSound(PLAYER_RESPAWN_LOCATION);
    SOUND::ENEMY_DAMAGED        = g_theAudio->CreateOrGetSound(ENEMY_DAMAGED_LOCATION);
    SOUND::ENEMY_DIES           = g_theAudio->CreateOrGetSound(ENEMY_DIES_LOCATION);
    SOUND::ASTEROIDS_DAMAGED    = g_theAudio->CreateOrGetSound(ASTEROIDS_DAMAGED_LOCATION);
}
