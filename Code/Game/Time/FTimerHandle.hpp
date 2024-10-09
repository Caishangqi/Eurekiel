#pragma once
#include "Game/Widget/WidgetHandler.hpp"

struct FTimerHandle
{
    float  times;
    void (*onFinishCallback)();

private:
    Game* owner;

public:
    FTimerHandle();
    ~FTimerHandle();

    void Update(float deltaSeconds);
    void SetOnFinishCallback(void (*callback)());
    void OnFinish(); // 在计时器完成时调用
    void SetOwner(Game* owner);
};

inline void FTimerHandle::SetOwner(Game* owner)
{
    this->owner = owner;
}

inline FTimerHandle::FTimerHandle()
    : times(0.f), onFinishCallback(nullptr)
{
}

inline FTimerHandle::~FTimerHandle()
{
    this->owner = nullptr;
}

inline void FTimerHandle::Update(float deltaSeconds)
{
    if (times > 0.f)
    {
        times -= deltaSeconds;
        if (times <= 0.f)
        {
            OnFinish();
        }
    }
}

inline void FTimerHandle::SetOnFinishCallback(void (*callback)())
{
    onFinishCallback = callback;
}

inline void FTimerHandle::OnFinish()
{
    if (onFinishCallback)
    {
        onFinishCallback();
    }

    owner->ReturnToMainMenu();
}
