#include "Mat44.hpp"

#include "MathUtils.hpp"
#include "Vec2.hpp"
#include "Vec3.hpp"
#include "Vec4.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

Mat44 Mat44::IDENTITY = Mat44();

Mat44::Mat44()
{
    for (int i = 0; i < 16; ++i)
    {
        m_values[i] = 0.f;
    }

    m_values[Ix] = 1.f;
    m_values[Jy] = 1.f;
    m_values[Kz] = 1.f;
    m_values[Tw] = 1.f;
}

Mat44::Mat44(const Vec2& iBasis2D, const Vec2& jBasis2D, const Vec2& translation2D) : Mat44()
{
    m_values[Ix] = iBasis2D.x;
    m_values[Iy] = iBasis2D.y;
    m_values[Jx] = jBasis2D.x;
    m_values[Jy] = jBasis2D.y;
    m_values[Tx] = translation2D.x;
    m_values[Ty] = translation2D.y;
}

Mat44::Mat44(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D, const Vec3& translation3D) : Mat44()
{
    m_values[Ix] = iBasis3D.x;
    m_values[Iy] = iBasis3D.y;
    m_values[Iz] = iBasis3D.z;
    m_values[Jx] = jBasis3D.x;
    m_values[Jy] = jBasis3D.y;
    m_values[Jz] = jBasis3D.z;
    m_values[Kx] = kBasis3D.x;
    m_values[Ky] = kBasis3D.y;
    m_values[Kz] = kBasis3D.z;
    m_values[Tx] = translation3D.x;
    m_values[Ty] = translation3D.y;
    m_values[Tz] = translation3D.z;
}

Mat44::Mat44(const Vec4& iBasis4D, const Vec4& jBasis4D, const Vec4& kBasis4D, const Vec4& translation4D) : Mat44()
{
    m_values[Ix] = iBasis4D.x;
    m_values[Iy] = iBasis4D.y;
    m_values[Iz] = iBasis4D.z;
    m_values[Iw] = iBasis4D.w;
    m_values[Jx] = jBasis4D.x;
    m_values[Jy] = jBasis4D.y;
    m_values[Jz] = jBasis4D.z;
    m_values[Jw] = jBasis4D.w;
    m_values[Kx] = kBasis4D.x;
    m_values[Ky] = kBasis4D.y;
    m_values[Kz] = kBasis4D.z;
    m_values[Kw] = kBasis4D.w;
    m_values[Tx] = translation4D.x;
    m_values[Ty] = translation4D.y;
    m_values[Tz] = translation4D.z;
    m_values[Tw] = translation4D.w;
}

Mat44::Mat44(const float* sixteenValuesBasisMajor)
{
    for (int i = 0; i < 16; ++i)
    {
        m_values[i] = sixteenValuesBasisMajor[i];
    }
}

const Mat44 Mat44::MakeTranslation2D(const Vec2& translationXY)
{
    Matrix44 result;

    result.m_values[Tx] = translationXY.x;
    result.m_values[Ty] = translationXY.y;

    return result;
}

const Mat44 Mat44::MakeTranslation3D(const Vec3& translationXYZ)
{
    Matrix44 result;

    result.m_values[Tx] = translationXYZ.x;
    result.m_values[Ty] = translationXYZ.y;
    result.m_values[Tz] = translationXYZ.z;

    return result;
}

const Mat44 Mat44::MakeUniformScale2D(float uniformScaleXY)
{
    Matrix44 result;

    result.m_values[Ix] = uniformScaleXY;
    result.m_values[Jy] = uniformScaleXY;

    return result;
}

const Mat44 Mat44::MakeUniformScale3D(float uniformScaleXYZ)
{
    Matrix44 result;

    result.m_values[Ix] = uniformScaleXYZ;
    result.m_values[Jy] = uniformScaleXYZ;
    result.m_values[Kz] = uniformScaleXYZ;

    return result;
}

const Mat44 Mat44::MakeNonUniformScale2D(const Vec2& nonUniformScaleXY)
{
    Matrix44 result;

    result.m_values[Ix] = nonUniformScaleXY.x;
    result.m_values[Jy] = nonUniformScaleXY.y;

    return result;
}

const Mat44 Mat44::MakeNonUniformScale3D(const Vec3& nonUniformScaleXYZ)
{
    Matrix44 result;

    result.m_values[Ix] = nonUniformScaleXYZ.x;
    result.m_values[Jy] = nonUniformScaleXYZ.y;
    result.m_values[Kz] = nonUniformScaleXYZ.z;

    return result;
}

const Mat44 Mat44::MakeZRotationDegrees(float rotationDegreesAboutZ)
{
    Matrix44 result;

    result.m_values[Ix] = CosDegrees(rotationDegreesAboutZ);
    result.m_values[Iy] = SinDegrees(rotationDegreesAboutZ);
    result.m_values[Jx] = -SinDegrees(rotationDegreesAboutZ);
    result.m_values[Jy] = CosDegrees(rotationDegreesAboutZ);

    return result;
}

const Mat44 Mat44::MakeYRotationDegrees(float rotationDegreesAboutY)
{
    Matrix44 result;

    result.m_values[Ix] = CosDegrees(rotationDegreesAboutY);
    result.m_values[Iz] = -SinDegrees(rotationDegreesAboutY);
    result.m_values[Kx] = SinDegrees(rotationDegreesAboutY);
    result.m_values[Kz] = CosDegrees(rotationDegreesAboutY);

    return result;
}

const Mat44 Mat44::MakeXRotationDegrees(float rotationDegreesAboutX)
{
    Matrix44 result;

    result.m_values[Jy] = CosDegrees(rotationDegreesAboutX);
    result.m_values[Jz] = SinDegrees(rotationDegreesAboutX);
    result.m_values[Ky] = -SinDegrees(rotationDegreesAboutX);
    result.m_values[Kz] = CosDegrees(rotationDegreesAboutX);


    return result;
}

const Mat44 Mat44::MakeOrthoProjection(float left, float right, float bottom, float top, float near, float far)
{
    Mat44 ortho;
    ortho.m_values[Ix] = 2.0f / (right - left);
    ortho.m_values[Jy] = 2.0f / (top - bottom);
    ortho.m_values[Kz] = 1.0f / (far - near);
    ortho.m_values[Tx] = -(right + left) / (right - left);
    ortho.m_values[Ty] = -(top + bottom) / (top - bottom);
    ortho.m_values[Tz] = -near / (far - near);
    ortho.m_values[Tw] = 1.0f;
    return ortho;
}

const Mat44 Mat44::MakePerspectiveProjection(float fovYDegrees, float aspect, float zNear, float zFar)
{
    Mat44 perspective;
    // TODO: ask ray about the formular
    perspective.m_values[Ix] = 1.0f / (aspect * tanf(ConvertDegreesToRadians(fovYDegrees * 0.5f)));
    perspective.m_values[Jy] = 1.0f / (tanf(ConvertDegreesToRadians(fovYDegrees * 0.5f)));
    perspective.m_values[Kz] = zFar / (zFar - zNear);
    perspective.m_values[Kw] = 1.0f;
    perspective.m_values[Tz] = -(zFar * zNear) / (zFar - zNear);
    perspective.m_values[Tw] = 0.0f;
    return perspective;
}

const Vec2 Mat44::TransformVectorQuantity2D(const Vec2& vectorQuantityXY) const
{
    float x = m_values[Ix] * vectorQuantityXY.x + m_values[Jx] * vectorQuantityXY.y;
    float y = m_values[Iy] * vectorQuantityXY.x + m_values[Jy] * vectorQuantityXY.y;

    return Vec2(x, y);
}

const Vec3 Mat44::TransformVectorQuantity3D(const Vec3& vectorQuantityXYZ) const
{
    float x = m_values[Ix] * vectorQuantityXYZ.x + m_values[Jx] * vectorQuantityXYZ.y + m_values[Kx] * vectorQuantityXYZ
        .z;
    float y = m_values[Iy] * vectorQuantityXYZ.x + m_values[Jy] * vectorQuantityXYZ.y + m_values[Ky] * vectorQuantityXYZ
        .z;
    float z = m_values[Iz] * vectorQuantityXYZ.x + m_values[Jz] * vectorQuantityXYZ.y + m_values[Kz] * vectorQuantityXYZ
        .z;
    return Vec3(x, y, z);
}

const Vec2 Mat44::TransformPosition2D(const Vec2& positionXY) const
{
    float x = m_values[Ix] * positionXY.x + m_values[Jx] * positionXY.y + m_values[Tx];
    float y = m_values[Iy] * positionXY.x + m_values[Jy] * positionXY.y + m_values[Ty];

    return Vec2(x, y);
}

const Vec3 Mat44::TransformPosition3D(const Vec3& position3D) const
{
    float x = m_values[Ix] * position3D.x + m_values[Jx] * position3D.y + m_values[Kx] * position3D.z + m_values[Tx];
    float y = m_values[Iy] * position3D.x + m_values[Jy] * position3D.y + m_values[Ky] * position3D.z + m_values[Ty];
    float z = m_values[Iz] * position3D.x + m_values[Jz] * position3D.y + m_values[Kz] * position3D.z + m_values[Tz];

    return Vec3(x, y, z);
}

const Vec4 Mat44::TransformHomogeneous3D(const Vec4& homogeneousPoint3D) const
{
    float x = m_values[Ix] * homogeneousPoint3D.x + m_values[Jx] * homogeneousPoint3D.y +
        m_values[Kx] * homogeneousPoint3D.z + m_values[Tx] * homogeneousPoint3D.w;
    float y = m_values[Iy] * homogeneousPoint3D.x + m_values[Jy] * homogeneousPoint3D.y +
        m_values[Ky] * homogeneousPoint3D.z + m_values[Ty] * homogeneousPoint3D.w;
    float z = m_values[Iz] * homogeneousPoint3D.x + m_values[Jz] * homogeneousPoint3D.y +
        m_values[Kz] * homogeneousPoint3D.z + m_values[Tz] * homogeneousPoint3D.w;
    float w = m_values[Iw] * homogeneousPoint3D.x + m_values[Jw] * homogeneousPoint3D.y +
        m_values[Kw] * homogeneousPoint3D.z + m_values[Tw] * homogeneousPoint3D.w;
    return Vec4(x, y, z, w);
}

float* Mat44::GetAsFloatArray()
{
    return m_values;
}

const float* Mat44::GetAsFloatArray() const
{
    return m_values;
}

Vec2 Mat44::GetIBasis2D() const
{
    return Vec2(m_values[Ix], m_values[Iy]);
}

Vec2 Mat44::GetJBasis2D() const
{
    return Vec2(m_values[Jx], m_values[Jy]);
}

Vec2 Mat44::GetTranslation2D() const
{
    return Vec2(m_values[Tx], m_values[Ty]);
}

Vec3 Mat44::GetIBasis3D() const
{
    return Vec3(m_values[Ix], m_values[Iy], m_values[Iz]);
}

Vec3 Mat44::GetJBasis3D() const
{
    return Vec3(m_values[Jx], m_values[Jy], m_values[Jz]);
}

Vec3 Mat44::GetKBasis3D() const
{
    return Vec3(m_values[Kx], m_values[Ky], m_values[Kz]);
}

Vec3 Mat44::GetTranslation3D() const
{
    return Vec3(m_values[Tx], m_values[Ty], m_values[Tz]);
}

Vec4 Mat44::GetIBasis4D() const
{
    return Vec4(m_values[Ix], m_values[Iy], m_values[Iz], m_values[Iw]);
}

Vec4 Mat44::GetJBasis4D() const
{
    return Vec4(m_values[Jx], m_values[Jy], m_values[Jz], m_values[Jw]);
}

Vec4 Mat44::GetKBasis4D() const
{
    return Vec4(m_values[Kx], m_values[Ky], m_values[Kz], m_values[Kw]);
}

Vec4 Mat44::GetTranslation4D() const
{
    return Vec4(m_values[Tx], m_values[Ty], m_values[Tz], m_values[Tw]);
}

const Mat44 Mat44::GetOrthonormalInverse() const
{
    // Extract rotation inversion
    Mat44 inv;
    inv.m_values[Ix] = m_values[Ix];
    inv.m_values[Jx] = m_values[Iy];
    inv.m_values[Kx] = m_values[Iz];

    inv.m_values[Iy] = m_values[Jx];
    inv.m_values[Jy] = m_values[Jy];
    inv.m_values[Ky] = m_values[Jz];

    inv.m_values[Iz] = m_values[Kx];
    inv.m_values[Jz] = m_values[Ky];
    inv.m_values[Kz] = m_values[Kz];

    inv.m_values[Iw] = 0.0f;
    inv.m_values[Jw] = 0.0f;
    inv.m_values[Kw] = 0.0f;

    // Need calculate the original matrix translation and add "-" to reverse that
    float newTx = -(m_values[Ix] * m_values[Tx] + m_values[Iy] * m_values[Ty] + m_values[Iz] * m_values[Tz]);
    float newTy = -(m_values[Jx] * m_values[Tx] + m_values[Jy] * m_values[Ty] + m_values[Jz] * m_values[Tz]);
    float newTz = -(m_values[Kx] * m_values[Tx] + m_values[Ky] * m_values[Ty] + m_values[Kz] * m_values[Tz]);

    inv.m_values[Tx] = newTx;
    inv.m_values[Ty] = newTy;
    inv.m_values[Tz] = newTz;

    inv.m_values[Tw] = 1.0f;

    return inv;
}

const Mat44 Mat44::GetInverse() const
{
    // [FIX] General 4x4 matrix inverse using Cramer's rule with cofactors
    // Works for ALL matrices including perspective projection
    float a00 = m_values[0],  a01 = m_values[1],  a02 = m_values[2],  a03 = m_values[3];
    float a10 = m_values[4],  a11 = m_values[5],  a12 = m_values[6],  a13 = m_values[7];
    float a20 = m_values[8],  a21 = m_values[9],  a22 = m_values[10], a23 = m_values[11];
    float a30 = m_values[12], a31 = m_values[13], a32 = m_values[14], a33 = m_values[15];

    // Calculate 2x2 determinants
    float b00 = a00 * a11 - a01 * a10;
    float b01 = a00 * a12 - a02 * a10;
    float b02 = a00 * a13 - a03 * a10;
    float b03 = a01 * a12 - a02 * a11;
    float b04 = a01 * a13 - a03 * a11;
    float b05 = a02 * a13 - a03 * a12;
    float b06 = a20 * a31 - a21 * a30;
    float b07 = a20 * a32 - a22 * a30;
    float b08 = a20 * a33 - a23 * a30;
    float b09 = a21 * a32 - a22 * a31;
    float b10 = a21 * a33 - a23 * a31;
    float b11 = a22 * a33 - a23 * a32;

    // Calculate determinant
    float det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

    if (fabsf(det) < 1e-10f)
    {
        // Singular matrix, return identity as fallback
        return Mat44::IDENTITY;
    }

    float invDet = 1.0f / det;

    Mat44 inv;
    inv.m_values[0]  = (a11 * b11 - a12 * b10 + a13 * b09) * invDet;
    inv.m_values[1]  = (-a01 * b11 + a02 * b10 - a03 * b09) * invDet;
    inv.m_values[2]  = (a31 * b05 - a32 * b04 + a33 * b03) * invDet;
    inv.m_values[3]  = (-a21 * b05 + a22 * b04 - a23 * b03) * invDet;
    inv.m_values[4]  = (-a10 * b11 + a12 * b08 - a13 * b07) * invDet;
    inv.m_values[5]  = (a00 * b11 - a02 * b08 + a03 * b07) * invDet;
    inv.m_values[6]  = (-a30 * b05 + a32 * b02 - a33 * b01) * invDet;
    inv.m_values[7]  = (a20 * b05 - a22 * b02 + a23 * b01) * invDet;
    inv.m_values[8]  = (a10 * b10 - a11 * b08 + a13 * b06) * invDet;
    inv.m_values[9]  = (-a00 * b10 + a01 * b08 - a03 * b06) * invDet;
    inv.m_values[10] = (a30 * b04 - a31 * b02 + a33 * b00) * invDet;
    inv.m_values[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * invDet;
    inv.m_values[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * invDet;
    inv.m_values[13] = (a00 * b09 - a01 * b07 + a02 * b06) * invDet;
    inv.m_values[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * invDet;
    inv.m_values[15] = (a20 * b03 - a21 * b01 + a22 * b00) * invDet;

    return inv;
}

const Mat44 Mat44::GetTranspose()
{
    Mat44 temp = *this;
    temp.Transpose();
    return temp;
}

void Mat44::SetTranslation2D(const Vec2& translationXY)
{
    m_values[Tx] = translationXY.x;
    m_values[Ty] = translationXY.y;
    m_values[Tz] = 0.0f;
    m_values[Tw] = 1.0f;
}

void Mat44::SetTranslation3D(const Vec3& translationXYZ)
{
    m_values[Tx] = translationXYZ.x;
    m_values[Ty] = translationXYZ.y;
    m_values[Tz] = translationXYZ.z;
    m_values[Tw] = 1.0f;
}

void Mat44::SetIJ2D(const Vec2& iBasis2D, const Vec2& jBasis2D)
{
    m_values[Ix] = iBasis2D.x;
    m_values[Iy] = iBasis2D.y;
    m_values[Iz] = 0.f;
    m_values[Iw] = 0.f;


    m_values[Jx] = jBasis2D.x;
    m_values[Jy] = jBasis2D.y;
    m_values[Jz] = 0.f;
    m_values[Jw] = 0.f;
}

void Mat44::SetIJTranslation2D(const Vec2& iBasis2D, const Vec2& jBasis2D, const Vec2& translationXY)
{
    SetIJ2D(iBasis2D, jBasis2D);
    SetTranslation2D(translationXY);
}

void Mat44::SetIJK3D(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D)
{
    m_values[Ix] = iBasis3D.x;
    m_values[Iy] = iBasis3D.y;
    m_values[Iz] = iBasis3D.z;
    m_values[Iw] = 0.f;

    m_values[Jx] = jBasis3D.x;
    m_values[Jy] = jBasis3D.y;
    m_values[Jz] = jBasis3D.z;
    m_values[Jw] = 0.f;

    m_values[Kx] = kBasis3D.x;
    m_values[Ky] = kBasis3D.y;
    m_values[Kz] = kBasis3D.z;
    m_values[Kw] = 0.f;
}

void Mat44::SetIJKTranslation3D(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D,
                                const Vec3& translationXYZ)
{
    SetIJK3D(iBasis3D, jBasis3D, kBasis3D);
    m_values[Tx] = translationXYZ.x;
    m_values[Ty] = translationXYZ.y;
    m_values[Tz] = translationXYZ.z;
    m_values[Tw] = 1.f;
}

void Mat44::SetIJKTranslation4D(const Vec4& iBasis4D, const Vec4& jBasis4D, const Vec4& kBasis4D,
                                const Vec4& translation4D)
{
    m_values[Ix] = iBasis4D.x;
    m_values[Iy] = iBasis4D.y;
    m_values[Iz] = iBasis4D.z;
    m_values[Iw] = iBasis4D.w;

    m_values[Jx] = jBasis4D.x;
    m_values[Jy] = jBasis4D.y;
    m_values[Jz] = jBasis4D.z;
    m_values[Jw] = jBasis4D.w;

    m_values[Kx] = kBasis4D.x;
    m_values[Ky] = kBasis4D.y;
    m_values[Kz] = kBasis4D.z;
    m_values[Kw] = kBasis4D.w;

    m_values[Tx] = translation4D.x;
    m_values[Ty] = translation4D.y;
    m_values[Tz] = translation4D.z;
    m_values[Tw] = translation4D.w;
}

void Mat44::Transpose()
{
    Mat44 temp;
    temp         = *this;
    m_values[Ix] = temp.m_values[Ix];
    m_values[Iy] = temp.m_values[Jx];
    m_values[Iz] = temp.m_values[Kx];
    m_values[Iw] = temp.m_values[Tx];

    m_values[Jx] = temp.m_values[Iy];
    m_values[Jy] = temp.m_values[Jy];
    m_values[Jz] = temp.m_values[Ky];
    m_values[Jw] = temp.m_values[Ty];

    m_values[Kx] = temp.m_values[Iz];
    m_values[Ky] = temp.m_values[Jz];
    m_values[Kz] = temp.m_values[Kz];
    m_values[Kw] = temp.m_values[Tz];

    m_values[Tx] = temp.m_values[Iw];
    m_values[Ty] = temp.m_values[Jw];
    m_values[Tz] = temp.m_values[Kw];
    m_values[Tw] = temp.m_values[Tw];
}

// TODO: view gain
void Mat44::Orthonormalize_XFwd_YLeft_ZUp()
{
    Vec3 original_i = GetIBasis3D();
    Vec3 original_j = GetJBasis3D();
    Vec3 original_k = GetKBasis3D();
    // Correction forward (i direction): full trust, only normalization
    Vec3 corrected_i = original_i.GetNormalized();

    // Correction up (k direction): remove the component on corrected_i from the original k, and then normalize
    float dotK_i      = DotProduct3D(original_k, corrected_i);
    Vec3  projK_on_i  = corrected_i * dotK_i;
    Vec3  corrected_k = (original_k - projK_on_i).GetNormalized();

    //Correct left (j direction): remove the components on corrected_i and corrected_k, and then normalize
    float dotJ_i      = DotProduct3D(original_j, corrected_i);
    float dotJ_k      = DotProduct3D(original_j, corrected_k);
    Vec3  projJ_on_i  = corrected_i * dotJ_i;
    Vec3  projJ_on_k  = corrected_k * dotJ_k;
    Vec3  corrected_j = (original_j - projJ_on_i - projJ_on_k).GetNormalized();

    m_values[Ix] = corrected_i.x;
    m_values[Iy] = corrected_i.y;
    m_values[Iz] = corrected_i.z;

    m_values[Jx] = corrected_j.x;
    m_values[Jy] = corrected_j.y;
    m_values[Jz] = corrected_j.z;

    m_values[Kx] = corrected_k.x;
    m_values[Ky] = corrected_k.y;
    m_values[Kz] = corrected_k.z;
}

void Mat44::Append(const Mat44& appendThis)
{
    // From https://youtu.be/2spTnAiQg4M?si=IJKe6t50iogzZAxW
    Mat44 oldMe = *this;

    m_values[Ix] = (oldMe.m_values[Ix] * appendThis.m_values[Ix]) +
        (oldMe.m_values[Jx] * appendThis.m_values[Iy]) +
        (oldMe.m_values[Kx] * appendThis.m_values[Iz]) +
        (oldMe.m_values[Tx] * appendThis.m_values[Iw]);

    m_values[Iy] = (oldMe.m_values[Iy] * appendThis.m_values[Ix]) +
        (oldMe.m_values[Jy] * appendThis.m_values[Iy]) +
        (oldMe.m_values[Ky] * appendThis.m_values[Iz]) +
        (oldMe.m_values[Ty] * appendThis.m_values[Iw]);

    m_values[Iz] = (oldMe.m_values[Iz] * appendThis.m_values[Ix]) +
        (oldMe.m_values[Jz] * appendThis.m_values[Iy]) +
        (oldMe.m_values[Kz] * appendThis.m_values[Iz]) +
        (oldMe.m_values[Tz] * appendThis.m_values[Iw]);

    m_values[Iw] = (oldMe.m_values[Iw] * appendThis.m_values[Ix]) +
        (oldMe.m_values[Jw] * appendThis.m_values[Iy]) +
        (oldMe.m_values[Kw] * appendThis.m_values[Iz]) +
        (oldMe.m_values[Tw] * appendThis.m_values[Iw]);


    m_values[Jx] = (oldMe.m_values[Ix] * appendThis.m_values[Jx]) +
        (oldMe.m_values[Jx] * appendThis.m_values[Jy]) +
        (oldMe.m_values[Kx] * appendThis.m_values[Jz]) +
        (oldMe.m_values[Tx] * appendThis.m_values[Jw]);

    m_values[Jy] = (oldMe.m_values[Iy] * appendThis.m_values[Jx]) +
        (oldMe.m_values[Jy] * appendThis.m_values[Jy]) +
        (oldMe.m_values[Ky] * appendThis.m_values[Jz]) +
        (oldMe.m_values[Ty] * appendThis.m_values[Jw]);

    m_values[Jz] = (oldMe.m_values[Iz] * appendThis.m_values[Jx]) +
        (oldMe.m_values[Jz] * appendThis.m_values[Jy]) +
        (oldMe.m_values[Kz] * appendThis.m_values[Jz]) +
        (oldMe.m_values[Tz] * appendThis.m_values[Jw]);

    m_values[Jw] = (oldMe.m_values[Iw] * appendThis.m_values[Jx]) +
        (oldMe.m_values[Jw] * appendThis.m_values[Jy]) +
        (oldMe.m_values[Kw] * appendThis.m_values[Jz]) +
        (oldMe.m_values[Tw] * appendThis.m_values[Jw]);

    m_values[Kx] = (oldMe.m_values[Ix] * appendThis.m_values[Kx]) +
        (oldMe.m_values[Jx] * appendThis.m_values[Ky]) +
        (oldMe.m_values[Kx] * appendThis.m_values[Kz]) +
        (oldMe.m_values[Tx] * appendThis.m_values[Kw]);

    m_values[Ky] = (oldMe.m_values[Iy] * appendThis.m_values[Kx]) +
        (oldMe.m_values[Jy] * appendThis.m_values[Ky]) +
        (oldMe.m_values[Ky] * appendThis.m_values[Kz]) +
        (oldMe.m_values[Ty] * appendThis.m_values[Kw]);

    m_values[Kz] = (oldMe.m_values[Iz] * appendThis.m_values[Kx]) +
        (oldMe.m_values[Jz] * appendThis.m_values[Ky]) +
        (oldMe.m_values[Kz] * appendThis.m_values[Kz]) +
        (oldMe.m_values[Tz] * appendThis.m_values[Kw]);

    m_values[Kw] = (oldMe.m_values[Iw] * appendThis.m_values[Kx]) +
        (oldMe.m_values[Jw] * appendThis.m_values[Ky]) +
        (oldMe.m_values[Kw] * appendThis.m_values[Kz]) +
        (oldMe.m_values[Tw] * appendThis.m_values[Kw]);

    m_values[Tx] = (oldMe.m_values[Ix] * appendThis.m_values[Tx]) +
        (oldMe.m_values[Jx] * appendThis.m_values[Ty]) +
        (oldMe.m_values[Kx] * appendThis.m_values[Tz]) +
        (oldMe.m_values[Tx] * appendThis.m_values[Tw]);

    m_values[Ty] = (oldMe.m_values[Iy] * appendThis.m_values[Tx]) +
        (oldMe.m_values[Jy] * appendThis.m_values[Ty]) +
        (oldMe.m_values[Ky] * appendThis.m_values[Tz]) +
        (oldMe.m_values[Ty] * appendThis.m_values[Tw]);

    m_values[Tz] = (oldMe.m_values[Iz] * appendThis.m_values[Tx]) +
        (oldMe.m_values[Jz] * appendThis.m_values[Ty]) +
        (oldMe.m_values[Kz] * appendThis.m_values[Tz]) +
        (oldMe.m_values[Tz] * appendThis.m_values[Tw]);

    m_values[Tw] = (oldMe.m_values[Iw] * appendThis.m_values[Tx]) +
        (oldMe.m_values[Jw] * appendThis.m_values[Ty]) +
        (oldMe.m_values[Kw] * appendThis.m_values[Tz]) +
        (oldMe.m_values[Tw] * appendThis.m_values[Tw]);
}


void Mat44::AppendZRotation(float degreesRotationAboutZ)
{
    Append(MakeZRotationDegrees(degreesRotationAboutZ));
}

void Mat44::AppendYRotation(float degreesRotationAboutY)
{
    Append(MakeYRotationDegrees(degreesRotationAboutY));
}

void Mat44::AppendXRotation(float degreesRotationAboutX)
{
    Append(MakeXRotationDegrees(degreesRotationAboutX));
}

void Mat44::AppendTranslation2D(const Vec2& translationXY)
{
    Append(MakeTranslation2D(translationXY));
}

void Mat44::AppendTranslation3D(const Vec3& translationXYZ)
{
    Append(MakeTranslation3D(translationXYZ));
}

void Mat44::AppendScaleUniform2D(float uniformScaleXY)
{
    Append(MakeUniformScale2D(uniformScaleXY));
}

void Mat44::AppendScaleUniform3D(float uniformScaleXYZ)
{
    Append(MakeUniformScale3D(uniformScaleXYZ));
}

void Mat44::AppendScaleNonUniform2D(const Vec2& nonUniformScaleXY)
{
    Append(MakeNonUniformScale2D(nonUniformScaleXY));
}

void Mat44::AppendScaleNonUniform3D(const Vec3& nonUniformScaleXYZ)
{
    Append(MakeNonUniformScale3D(nonUniformScaleXYZ));
}
