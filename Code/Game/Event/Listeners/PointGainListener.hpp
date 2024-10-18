#pragma once
#include "Game/Event/Listener.hpp"
#include "Game/Event/Events/PointGainEvent.hpp"

class PointGainListener : public TemplateListener<PointGainEvent>
{
protected:
    void onEvent(PointGainEvent* event) override;
};
