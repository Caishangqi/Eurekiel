#pragma once

struct Vec2;
struct Vertex_PCU;


void TransformVertexArrayXY3D(int numVerts, Vertex_PCU* verts, float uniformScaleXY,
                              float rotationDegreesAboutZ, Vec2 const& translationXY);
