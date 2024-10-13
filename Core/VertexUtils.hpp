#pragma once

struct Vec2;
struct Vertex_PCU;


void TransformVertexArrayXY3D(int   numVerts, Vertex_PCU*              verts, float uniformScaleXY,
                              float rotationDegreesAboutZ, const Vec2& translationXY);
