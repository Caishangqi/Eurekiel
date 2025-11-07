#pragma once
// Add Homogeneous 2D/3D transformation matrix, stored basis-major in memory (Ix,Iy,Iz,Iw,Jx,Jy,...)
//
// Note: we specifically do NOT provide an operator* overload, since doing so would require a
// decision on whether the class only work correctly with either column-major or row-major style
// of any rotation. There are two different ways of writing operator*, and in order to implement
// an operator*, we are forced to make a notational commitment. This is certainly ambiguous to the
// end consumer. At the very least, probably confusing. Instead, we prefer to use method names,
// such as `Append`, which are more neutral (e.g. multiply a new matrix "on the right in column-
// notation" or on the left in row-notation).

struct Vec2;
struct Vec3;
struct Vec4;

struct Mat44
{
    static Mat44 IDENTITY;

    enum { Ix, Iy, Iz, Iw, Jx, Jy, Jz, Jw, Kx, Ky, Kz, Kw, Tx, Ty, Tz, Tw }; // index nicknames, [0] thru [15]
    float m_values[16]; // stored in "basis major" order (Ix,Iy,Iz,Iw,Jx,Jy,... translation in [12,13,14]

    Mat44(); // Default ctor - returns the IDENTITY matrix!
    explicit Mat44(const Vec2& iBasis2D, const Vec2& jBasis2D, const Vec2& translation2D);
    explicit Mat44(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D, const Vec3& translation3D);
    explicit Mat44(const Vec4& iBasis4D, const Vec4& jBasis4D, const Vec4& kBasis4D, const Vec4& translation4D);
    explicit Mat44(const float* sixteenValuesBasisMajor);

    static const Mat44 MakeTranslation2D(const Vec2& translationXY);
    static const Mat44 MakeTranslation3D(const Vec3& translationXYZ);
    static const Mat44 MakeUniformScale2D(float uniformScaleXY);
    static const Mat44 MakeUniformScale3D(float uniformScaleXYZ);
    static const Mat44 MakeNonUniformScale2D(const Vec2& nonUniformScaleXY);
    static const Mat44 MakeNonUniformScale3D(const Vec3& nonUniformScaleXYZ);
    static const Mat44 MakeZRotationDegrees(float rotationDegreesAboutZ);
    static const Mat44 MakeYRotationDegrees(float rotationDegreesAboutY);
    static const Mat44 MakeXRotationDegrees(float rotationDegreesAboutX);
    /**
     * Creates a matrix which will transform points in ortho (screen/world) space to D3D11 NDC (Normalized Device Coordinates)
     * space: -1 to +1 on X and Y, and 0 to 1 on Z)
     * @param left Left 
     * @param right 
     * @param bottom 
     * @param top 
     * @param near 
     * @param far 
     * @return 
     */
    static const Mat44 MakeOrthoProjection(float left, float right, float bottom, float top, float near, float far);
    /**
     * Creates a matrix which will set up points for a 3D perspective illusion by (among other things) setting up their W
     * components in preparation for the automatic GPU w-divide step.  zNear and zFar are both positive non-zero numbers
     * (view frustum near and far plane distances).
     * @param fovYDegrees 
     * @param aspect 
     * @param zNear 
     * @param zFar 
     * @return 
     */
    static const Mat44 MakePerspectiveProjection(float fovYDegrees, float aspect, float zNear, float zFar);

    /// TODO: Make the Look at Matrix inside the STATIC method 

    const Vec2 TransformVectorQuantity2D(const Vec2& vectorQuantityXY) const; // assumes z=0, w=0
    const Vec3 TransformVectorQuantity3D(const Vec3& vectorQuantityXYZ) const; // assumes w=0
    const Vec2 TransformPosition2D(const Vec2& positionXY) const; // assumes z=0, w=1
    const Vec3 TransformPosition3D(const Vec3& position3D) const; // assumes w=1
    const Vec4 TransformHomogeneous3D(const Vec4& homogeneousPoint3D) const; // w is provided

    float*       GetAsFloatArray(); // non-const (mutable) version
    const float* GetAsFloatArray() const; // const version, used only when Mat44 is const

    Vec2 GetIBasis2D() const;
    Vec2 GetJBasis2D() const;
    Vec2 GetTranslation2D() const;

    Vec3 GetIBasis3D() const;
    Vec3 GetJBasis3D() const;
    Vec3 GetKBasis3D() const;
    Vec3 GetTranslation3D() const;
    Vec4 GetTranslation4D() const;
    /// Creates a matrix which is the inverse of the matrix being asked, so long as that matrix is orthonormal (see above).
    /// A full general-case matrix inversion function that works with non-orthonormal matrices can be added later, but it
    /// will be both slower and more complex.
    const Mat44 GetOrthonormalInverse() const;
    const Mat44 GetInverse() const;
    const Mat44 GetTranspose();

    Vec4 GetIBasis4D() const;
    Vec4 GetJBasis4D() const;
    Vec4 GetKBasis4D() const;

    void SetTranslation2D(const Vec2& translationXY); // Sets translation z = 0, translation w = 1
    void SetTranslation3D(const Vec3& translationXYZ); // Sets translation w = 1
    void SetIJ2D(const Vec2& iBasis2D, const Vec2& jBasis2D); // Sets z=0, w=1 for i & j; does not modify x or
    void SetIJTranslation2D(const Vec2& iBasis2D, const Vec2& jBasis2D, const Vec2& translationXY); // Sets z=0, w=0 for i & j; w = 1 for t; does not modify k
    void SetIJK3D(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D); // Sets w=0 for ijk; does not modify t
    void SetIJKTranslation3D(const Vec3& iBasis3D, const Vec3& jBasis3D, const Vec3& kBasis3D, const Vec3& translationXYZ); // Sets w=1 for t
    void SetIJKTranslation4D(const Vec4& iBasis4D, const Vec4& jBasis4D, const Vec4& kBasis4D, const Vec4& translation4D); // All 16 values provided
    /// modified the matrix in question, swapping its rows and its columns (i.e. flipping it diagonally)
    void Transpose(); // swaps columns with rows
    ///  “corrects” a matrix which may or may not currently be orthonormal; it tries to create the best/closest orthonormal
    ///  matrix it can to the original matrix values. 
    void Orthonormalize_XFwd_YLeft_ZUp(); // Forward is canonical, Up is secondary, Left tertiary

    void Append(const Mat44& appendThis); // multiply on right in column notation / on left in row notation
    void AppendZRotation(float degreesRotationAboutZ); // same as appending (+= in column notation) z-rotation matrix
    void AppendYRotation(float degreesRotationAboutY); // same as appending (+= in column notation) y-rotation matrix
    void AppendXRotation(float degreesRotationAboutX); // same as appending (+= in column notation) x-rotation matrix
    void AppendTranslation2D(const Vec2& translationXY); // same as appending (+= in column notation) a translation matrix
    void AppendTranslation3D(const Vec3& translationXYZ); // same as appending (+= in column notation) a translation matrix
    void AppendScaleUniform2D(float uniformScaleXY); // i and j bases should remain unaffected
    void AppendScaleUniform3D(float uniformScaleXYZ); // i and j and k bases should remain unaffected
    void AppendScaleNonUniform2D(const Vec2& nonUniformScaleXY); // translation should remain unaffected
    void AppendScaleNonUniform3D(const Vec3& nonUniformScaleXYZ); // i and t bases should remain unaffected
};

using Matrix44 = Mat44;
