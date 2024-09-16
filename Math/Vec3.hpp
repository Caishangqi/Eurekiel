struct Vec3
{
public:
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;

public:
    Vec3() = default;
    explicit Vec3( float initialX, float initialY, float initialZ );

    // Accessors (const methods)
    float        GetLength() const;
    float        GetLengthXY() const;
    float        GetLengthSquared() const;
    float        GetLengthXYSquared() const;
    float        GetAngleAboutZRadians() const;
    float        GetAngleAboutZDegrees() const;
    Vec3 const   GetRotatedAboutZRadians( float deltaRadians ) const;
    Vec3 const   GetRotatedAboutZDegrees( float deltaDegrees ) const;
    Vec3 const   GetClamped( float maxLength ) const;
    Vec3 const   GetNormalized() const;

    // Operators (const)
    bool         operator==( Vec3 const& compare ) const;    // vec3 == vec3
    bool         operator!=( Vec3 const& compare ) const;    // vec3 != vec3
    Vec3 const   operator+( Vec3 const& vecToAdd ) const;    // vec3 + vec3
    Vec3 const   operator-( Vec3 const& vecToSubtract ) const; // vec3 - vec3
    Vec3 const   operator*( float uniformScale ) const;      // vec3 * float
    Vec3 const   operator/( float inverseScale ) const;      // vec3 / float

    // Operators (self-mutating / non-const)
    void         operator+=( Vec3 const& vecToAdd );         // vec3 += vec3
    void         operator-=( Vec3 const& vecToSubtract );    // vec3 -= vec3
    void         operator*=( float uniformScale );           // vec3 *= float
    void         operator/=( float uniformDivisor );         // vec3 /= float
    void         operator=( Vec3 const& copyFrom );          // vec3 = vec3

    // Standalone "friend" functions that are conceptually, but not actually, part of Vec3::
    friend Vec3 const   operator*( float uniformScale, Vec3 const& vecToScale );  // float * vec3
};
