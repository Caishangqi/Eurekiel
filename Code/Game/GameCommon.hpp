#pragma once

struct Vec2;
struct Rgba8;

class App;
class Renderer;
class RandomNumberGenerator;

extern RandomNumberGenerator* g_rng;
extern App* g_theApp;
extern Renderer* g_renderer;

constexpr int NUM_STARTING_ASTEROIDS = 6;
constexpr int MAX_ASTEROIDS = 12;
constexpr int MAX_BULLETS = 20;
constexpr float WORLD_SIZE_X = 200.f;
constexpr float WORLD_SIZE_Y = 100.f;
constexpr float WORLD_CENTER_X = WORLD_SIZE_X / 2.f;
constexpr float WORLD_CENTER_Y = WORLD_SIZE_Y / 2.f;
constexpr float ASTEROID_SPEED = 10.f;
constexpr float ASTEROID_PHYSICS_RADIUS = 1.6f;
constexpr float ASTEROID_COSMETIC_RADIUS = 2.0f;
constexpr float BULLET_LIFETIME_SECONDS = 2.0f;
constexpr float BULLET_SPEED = 50.f;
constexpr float BULLET_PHYSICS_RADIUS = 0.5f;
constexpr float BULLET_COSMETIC_RADIUS = 2.0f;
constexpr float PLAYER_SHIP_ACCELERATION = 30.f;
constexpr float PLAYER_SHIP_TURN_SPEED = 300.f;
constexpr float PLAYER_SHIP_PHYSICS_RADIUS = 1.75f;
constexpr float PLAYER_SHIP_COSMETIC_RADIUS = 2.25f;

void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);

void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color);


class GameCommon
{
public:
};
