#pragma once
#include <vector>

#include "BaseWidget.hpp"
struct Vertex_PCU;

class WidgetInGameScoreBoard : public BaseWidget
{
public:
    WidgetInGameScoreBoard(WidgetHandler* handler);
    ~WidgetInGameScoreBoard() override;

    void Update(float deltaSeconds) override;
    void Render() override;

private:
    int m_Score = 0.f;
    int m_GoalScore = 0.f;

    std::vector<Vertex_PCU> textCurrentScore;
    std::vector<Vertex_PCU> textGoalScore;
};
