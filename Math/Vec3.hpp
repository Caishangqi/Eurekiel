#pragma once

struct Vec3
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    ~Vec3();
    Vec3();
    explicit Vec3(float initialX, float initialY, float initialZ);
    Vec3(const Vec3& copyFrom);
};
