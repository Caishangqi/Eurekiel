#pragma once
#include "BaseWidget.hpp"
class PlayerShip;

class WidgetPlayerHealth : public BaseWidget
{
public:
    WidgetPlayerHealth(WidgetHandler* handler);
    ~WidgetPlayerHealth() override;
    void OnPlayerShipRespawn(PlayerShip* playerShip, int remainTry);
    void Update(float deltaSeconds) override;

    void Render() override;

    void Reset() override;

    // health bar
};
