#include "VertexUtils.hpp"
#include "Vertex_PCU.hpp"
#include "Engine/Math/MathUtils.hpp"

void TransformVertexArrayXY3D(int numVerts, Vertex_PCU* verts, float uniformScaleXY, float rotationDegreesAboutZ,
                              const Vec2& translationXY)
{
    for (int vertIndex = 0; vertIndex < numVerts; vertIndex++)
    {
        Vec3& pos = verts[vertIndex].m_position;
        TransformPositionXY3D(pos, uniformScaleXY, rotationDegreesAboutZ, translationXY);
    }
}
