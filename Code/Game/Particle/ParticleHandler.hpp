#pragma once

enum EShape : int;
struct FParticleProperty;
class BaseParticle;
struct Rgba8;
struct Vec2;

class ParticleHandler
{
protected:
    ParticleHandler();
    static ParticleHandler* instance_;
    BaseParticle*           m_baseParticle[1024] = {};

public:
    ParticleHandler(const ParticleHandler& other)             = delete;
    void                    operator=(const ParticleHandler&) = delete;
    static ParticleHandler* getInstance();
    void                    Update(float deltaTime);
    void                    Render();
    void                    HandleParticleCollision();
    void                    GarbageCollection();
    /**
     * Clean all particles in the current scene
     */
    void CleanParticle();

    void SpawnNewParticleCluster(FParticleProperty property);

    void SpawnNewParticle(FParticleProperty property);
};
