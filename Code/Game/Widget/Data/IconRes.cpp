#include "IconRes.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec3.hpp"


Vertex_PCU ICON::SPACESHIP[15] = {
    Vertex_PCU(Vec3(1.f, 0.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(2.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, 2.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(-2.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(2.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(-2.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, -2.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(-2.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(0.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(-2.f, 1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
    Vertex_PCU(Vec3(-2.f, -1.f, 0.f), Rgba8(102, 153, 204), Vec2()),
};

Vertex_PCU ICON::SPACESHIP_TAIL_FLAME[12] = {
    // RED
    Vertex_PCU(Vec3(-4.f, 1.f, 0.f), Rgba8(224, 218, 136, 180), Vec2()),
    Vertex_PCU(Vec3(-2.f, 1.f, 0.f), Rgba8(224, 218, 136), Vec2()),
    Vertex_PCU(Vec3(-2.f, -1.f, 0.f), Rgba8(224, 218, 136), Vec2()),
    // GREEN
    Vertex_PCU(Vec3(-4.f, 1.f, 0.f), Rgba8(224, 218, 136, 180), Vec2()),
    Vertex_PCU(Vec3(-4.f, -1.f, 0.f), Rgba8(224, 218, 136, 180), Vec2()),
    Vertex_PCU(Vec3(-2.f, -1.f, 0.f), Rgba8(224, 218, 136), Vec2()),
    // ORANGE
    Vertex_PCU(Vec3(-4.f, .5f, 0.f), Rgba8(230, 167, 60, 160), Vec2()),
    Vertex_PCU(Vec3(-6.f, -.5f, 0.f), Rgba8(230, 167, 60, 20), Vec2()),
    Vertex_PCU(Vec3(-6.f, .5f, 0.f), Rgba8(230, 167, 60, 20), Vec2()),
    // PINK
    Vertex_PCU(Vec3(-4.f, .5f, 0.f), Rgba8(230, 167, 60, 160), Vec2()),
    Vertex_PCU(Vec3(-4.f, -.5f, 0.f), Rgba8(230, 167, 60, 160), Vec2()),
    Vertex_PCU(Vec3(-6.f, -.5f, 0.f), Rgba8(230, 167, 60, 20), Vec2()),
};

Vertex_PCU ICON::CUBE[6] = {
    Vertex_PCU(Vec3(2.25, 2.25, 0.f), Rgba8(230, 167, 60), Vec2()),
    Vertex_PCU(Vec3(2.25, -2.25, 0.f), Rgba8(230, 167, 60), Vec2()),
    Vertex_PCU(Vec3(-2.25, -2.25, 0.f), Rgba8(230, 167, 60), Vec2()),

    Vertex_PCU(Vec3(2.25, 2.25, 0.f), Rgba8(230, 167, 60), Vec2()),
    Vertex_PCU(Vec3(-2.25, -2.25, 0.f), Rgba8(230, 167, 60), Vec2()),
    Vertex_PCU(Vec3(-2.25, 2.25, 0.f), Rgba8(230, 167, 60), Vec2()),
};

Vertex_PCU ICON::TEXT_SPACESHIP_TRIANGLES[36] = {
    // S (分为多个三角形)
    // Top-left pixel (2 triangles)
    Vertex_PCU(Vec3(-4.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(-3.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-4.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(-3.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-4.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(-3.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Middle-left pixel (2 triangles)
    Vertex_PCU(Vec3(-4.f, 0.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(-3.f, 0.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-4.f, -1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(-3.f, 0.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-4.f, -1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(-3.f, -1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Repeat for the rest of the "S"

    // P (Top-left pixel of 'P')
    Vertex_PCU(Vec3(-1.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(0.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-1.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(0.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(-1.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(0.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Repeat for the rest of the 'P'

    // A (Top-left pixel of 'A')
    Vertex_PCU(Vec3(2.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(3.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(2.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(3.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(2.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(3.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Repeat for the rest of the 'A'

    // C
    Vertex_PCU(Vec3(5.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(6.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(5.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(6.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(5.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(6.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Repeat for the rest of the 'C'

    // E
    Vertex_PCU(Vec3(7.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p1
    Vertex_PCU(Vec3(8.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(7.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3

    Vertex_PCU(Vec3(8.f, 2.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p2
    Vertex_PCU(Vec3(7.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p3
    Vertex_PCU(Vec3(8.f, 1.f, 0.f), Rgba8(255, 255, 255), Vec2()), // p4

    // Repeat for the rest of the 'E'

    // Repeat the same logic for the rest of the letters 'S', 'H', 'I', 'P'
};
