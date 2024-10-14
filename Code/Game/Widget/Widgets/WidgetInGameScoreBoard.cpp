#include "WidgetInGameScoreBoard.hpp"

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Level/LevelHandler.hpp"
#include "Game/Widget/WidgetHandler.hpp"

WidgetInGameScoreBoard::WidgetInGameScoreBoard(WidgetHandler* handler): BaseWidget(handler)
{
    printf("[widget]        > WidgetInGameScoreBoard Registered!\n");
    name    = "InGameScoreBoard";
    visible = false;
    active  = false;
}

WidgetInGameScoreBoard::~WidgetInGameScoreBoard()
{
}

void WidgetInGameScoreBoard::Update(float deltaSeconds)
{
    BaseWidget::Update(deltaSeconds);
    if (active)
    {
        Game* game = handler->owner;
        m_Score    = game->Score;

        if (game->m_levelHandler->GetCurrentLevel())
        {
            m_GoalScore = game->m_levelHandler->GetCurrentLevel()->goalScore;
        }
    }
}

void WidgetInGameScoreBoard::Render()
{
    BaseWidget::Render();

    if (visible)
    {
        AddVertsForTextTriangles2D(textCurrentScore, "Score: " + std::to_string(m_Score), Vec2(420.f, 760.f), 30.f,
                                   Rgba8(255, 255, 255));
        g_renderer->DrawVertexArray(textCurrentScore.size(), textCurrentScore.data());
        textCurrentScore.clear();


        AddVertsForTextTriangles2D(textGoalScore, "Level Goal Score (" + std::to_string(m_GoalScore) + ")",
                                   Vec2(820.f, 760.f),
                                   30.f,
                                   Rgba8(255, 255, 255));
        g_renderer->DrawVertexArray(textGoalScore.size(), textGoalScore.data());
        textGoalScore.clear();
    }
}
