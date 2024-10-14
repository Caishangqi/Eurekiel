#pragma once

#include "Engine/Audio/AudioSystem.hpp"

#define PLAYER_SHOOTS_BULLET_LOCATION "Data/Audio/PlayerShootBullet.wav"
#define PLAYER_DIES_LOCATION "Data/Audio/PlayerDies.wav"
#define PLAYER_DAMAGED_LOCATION "Data/Audio/PlayerDamaged.wav"
#define PLAYER_RESPAWN_LOCATION "Data/Audio/PlayerRespawn.wav"
#define ENEMY_DAMAGED_LOCATION "Data/Audio/EnemyDamaged.wav"
#define ASTEROIDS_DAMAGED_LOCATION "Data/Audio/AsteroidsDamaged.wav"
#define ENEMY_DIES_LOCATION "Data/Audio/EnemyDies.wav"
#define DESTROY_TETROMINO_LOCATION "Data/Audio/DestroyTetromino.mp3"
#define POINT_ADD_LOCATION "Data/Audio/PointAdd.mp3"

namespace SOUND
{
    extern SoundID PLAYER_SHOOTS_BULLET;
    extern SoundID PLAYER_DIES;
    extern SoundID PLAYER_DAMAGED;
    extern SoundID PLAYER_RESPAWN;
    extern SoundID ENEMY_DIES;
    extern SoundID ENEMY_DAMAGED;
    extern SoundID ASTEROIDS_DAMAGED;
    extern SoundID NEW_WAVE;
    extern SoundID GAME_OVER_WIN;
    extern SoundID GAME_OVER_LOSE;
    extern SoundID MAIN_MENU;
    extern SoundID DESTROY_TETROMINO;
    extern SoundID POINT_ADD;
}
