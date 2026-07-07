#pragma once

#include "MathReview_Harness.hpp"
#include "core/math/Matrix4x4.hpp"
#include <cmath>

namespace review::math {

// Helper: compare two 4x4 matrices with a tolerance.  Used since Matrix4x4's
// operator== is bitwise (no tolerance), but expected values derived from
// transcendentals like sin/cos need ~1e-6 slop.
inline bool matrixNear(const ST::Matrix4x4& a, const ST::Matrix4x4& b, float tol) {
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (std::fabs(a.m[i][j] - b.m[i][j]) > tol) return false;
    return true;
}

// =============================================================
// Matrix4x4 - identity / arithmetic / vector multiply
// =============================================================
inline void test_Matrix4x4_Identity(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector4;

    Matrix4x4 I = Matrix4x4::identity();
    Matrix4x4 Z = Matrix4x4::zero();

    // Identity is the default constructor.
    EXPECT_TRUE(ctx, Matrix4x4() == Matrix4x4::identity());
    EXPECT_TRUE(ctx, I == Matrix4x4::identity());
    EXPECT_TRUE(ctx, Matrix4x4::identity() != Z);

    // Identity * v == v (homogeneous; w=1 stays 1, w=0 stays 0)
    Vector4 v1(2.0f, 3.0f, 4.0f, 1.0f);
    EXPECT_TRUE(ctx, (I * v1) == v1);

    Vector4 v0(5.0f, 6.0f, 7.0f, 0.0f);
    EXPECT_TRUE(ctx, (I * v0) == v0);

    // Identity multiplication: A * I == A
    Matrix4x4 A = Matrix4x4::translation(ST::Vector3(1, 2, 3));
    EXPECT_TRUE(ctx, (A * I) == A);
    EXPECT_TRUE(ctx, (I * A) == A);

    // Zero * v == 0 in all four components, including w.  The zero matrix
    // has no m[3][3] == 1, so the "homogeneous w stays 1" trick does not
    // apply -- multiplying any vector by an all-zero matrix annihilates it.
    EXPECT_TRUE(ctx, (Z * v1) == Vector4(0, 0, 0, 0));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

inline void test_Matrix4x4_Transpose(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;

    // Build a non-symmetric matrix manually.
    Matrix4x4 M;
    M.m[0][0] = 1; M.m[0][1] = 2; M.m[0][2] = 3; M.m[0][3] = 4;
    M.m[1][0] = 5; M.m[1][1] = 6; M.m[1][2] = 7; M.m[1][3] = 8;
    M.m[2][0] = 9; M.m[2][1] =10; M.m[2][2] =11; M.m[2][3] =12;
    M.m[3][0] =13; M.m[3][1] =14; M.m[3][2] =15; M.m[3][3] =16;

    Matrix4x4 Mt = M.transpose();

    // (Mt)[i][j] == M[j][i]
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            EXPECT_NEAR(ctx, Mt.m[i][j], M.m[j][i], ST::EPSILON);
        }
    }

    // Double transpose = identity of the transpose op
    EXPECT_TRUE(ctx, Mt.transpose() == M);

    // Identity is symmetric
    EXPECT_TRUE(ctx, Matrix4x4::identity().transpose() == Matrix4x4::identity());

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

inline void test_Matrix4x4_Inverse(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector4;

    // A * A^-1 == I  (sample of non-trivial matrices)
    auto checkInverse = [&](const Matrix4x4& A, const char* what) {
        Matrix4x4 Ainv = A.inverse();

        // If A is singular, Matrix4x4::inverse() returns identity by contract
        // (see the "< 1e-10" pivot branch).  Detect that and treat singular
        // input as out-of-scope for this assertion.
        bool returnedIdentityDueToSingular =
            (Ainv == Matrix4x4::identity()) && (A != Matrix4x4::identity());
        if (returnedIdentityDueToSingular) {
            EXPECT_TRUE(ctx, false); // silently skip -- see comment below
            return;
        }

        Matrix4x4 product = A * Ainv;
        if (!matrixNear(product, Matrix4x4::identity(), 1e-4f)) {
            ++ctx.checks; ctx.currentFailed = true;
            ctx.log << "    [FAIL] " << what << ": A * Ainv != I "
                    << "(max component off by > 1e-4)\n";
        } else {
            ++ctx.checks;
        }
    };

    // Translation
    checkInverse(Matrix4x4::translation(ST::Vector3(5, -3, 7)), "translation(5,-3,7)");
    // Scale
    checkInverse(Matrix4x4::scale(2.0f, 3.0f, 4.0f), "scale(2,3,4)");
    // Rotation
    checkInverse(Matrix4x4::rotationX(ST::degreesToRadians(37.0f)), "rotationX(37deg)");
    checkInverse(Matrix4x4::rotationY(ST::degreesToRadians(-23.5f)), "rotationY(-23.5deg)");
    checkInverse(Matrix4x4::rotationZ(ST::degreesToRadians(89.0f)), "rotationZ(89deg)");

    // Combined: S * R * T is a typical model matrix.  Build by hand:
    Matrix4x4 model = Matrix4x4::scale(1.5f, 2.0f, 0.5f) *
                      Matrix4x4::rotationY(ST::degreesToRadians(45.0f)) *
                      Matrix4x4::translation(ST::Vector3(1, 2, 3));
    checkInverse(model, "S * R * T combined");

    // Identity inverse is identity.
    EXPECT_TRUE(ctx, Matrix4x4::identity().inverse() == Matrix4x4::identity());

    // Apply inverse to a point that A transformed away: should round-trip.
    Matrix4x4 T = Matrix4x4::translation(ST::Vector3(10, -5, 7));
    ST::Vector3 p(1, 2, 3);
    ST::Vector3 moved   = T.transformPoint(p);
    ST::Vector3 back    = T.inverse().transformPoint(moved);
    EXPECT_TRUE(ctx,
        std::fabs(back.x - p.x) <= 1e-4f &&
        std::fabs(back.y - p.y) <= 1e-4f &&
        std::fabs(back.z - p.z) <= 1e-4f);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// =============================================================
// Factory matrices: translation / scale / rotation
// =============================================================
inline void test_Matrix4x4_Translation(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector3;
    using ST::Vector4;

    Matrix4x4 T = Matrix4x4::translation(Vector3(7, -3, 2));

    // Layout sanity: translation lives in the last column (m[*][3]).
    EXPECT_NEAR(ctx, T.m[0][3],  7.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, T.m[1][3], -3.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, T.m[2][3],  2.0f, ST::EPSILON);

    // The homogeneous row m[3][*] is (0,0,0,1) -- distinguishing points from
    // directions.  Lock this so the contract can't drift.
    EXPECT_NEAR(ctx, T.m[3][0], 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, T.m[3][1], 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, T.m[3][2], 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, T.m[3][3], 1.0f, ST::EPSILON);

    // transformPoint moves a point by (7,-3,2).
    Vector3 p(1, 1, 1);
    Vector3 moved = T.transformPoint(p);
    EXPECT_TRUE(ctx, moved == Vector3(8, -2, 3));

    // transformDirection must NOT translate a direction.
    Vector3 dir(1, 0, 0);
    Vector3 movedDir = T.transformDirection(dir);
    EXPECT_TRUE(ctx, movedDir == dir);

    // transformPoint on the origin is purely the translation.
    EXPECT_TRUE(ctx, T.transformPoint(Vector3(0, 0, 0)) == Vector3(7, -3, 2));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

inline void test_Matrix4x4_Scale(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector3;

    Matrix4x4 S = Matrix4x4::scale(2.0f, 3.0f, 4.0f);

    EXPECT_NEAR(ctx, S.m[0][0], 2.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, S.m[1][1], 3.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, S.m[2][2], 4.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, S.m[3][3], 1.0f, ST::EPSILON);

    Vector3 p(1, 1, 1);
    EXPECT_TRUE(ctx, S.transformPoint(p) == Vector3(2, 3, 4));

    Matrix4x4 Su = Matrix4x4::scale(5.0f); // uniform form
    EXPECT_NEAR(ctx, Su.m[0][0], 5.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, Su.m[1][1], 5.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, Su.m[2][2], 5.0f, ST::EPSILON);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

inline void test_Matrix4x4_Rotation(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector3;

    // Rotation must be orthogonal: det == +1, preserves lengths.
    auto checkOrthogonal = [&](const Matrix4x4& R, const char* what) {
        Matrix4x4 RtR = R.transpose() * R;
        if (!matrixNear(RtR, Matrix4x4::identity(), 1e-4f)) {
            ++ctx.checks; ctx.currentFailed = true;
            ctx.log << "    [FAIL] " << what << ": R^T * R != I\n";
        } else {
            ++ctx.checks;
        }
    };

    Matrix4x4 Rx90 = Matrix4x4::rotationX(ST::degreesToRadians(90.0f));
    Matrix4x4 Ry45 = Matrix4x4::rotationY(ST::degreesToRadians(45.0f));
    Matrix4x4 Rz30 = Matrix4x4::rotationZ(ST::degreesToRadians(30.0f));

    checkOrthogonal(Rx90, "rotationX(90)");
    checkOrthogonal(Ry45, "rotationY(45)");
    checkOrthogonal(Rz30, "rotationZ(30)");

    // rotationX(90deg): y-axis -> z-axis, z-axis -> -y-axis
    //
    // These involve cos(pi/2) and sin(pi/2) and so cannot be compared
    // bitwise -- IEEE-754 gives ~6.12e-17 residual.  Use EXPECT_NEAR
    // per component, matching the convention already used elsewhere
    // in this file (checkOrthogonal above, RzFull below).
    Vector3 yAxis(0, 1, 0);
    Vector3 yRot = Rx90.transformDirection(yAxis);
    EXPECT_NEAR(ctx, yRot.x, 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yRot.y, 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yRot.z, 1.0f, ST::EPSILON);

    Vector3 zAxis(0, 0, 1);
    Vector3 zRot = Rx90.transformDirection(zAxis);
    EXPECT_NEAR(ctx, zRot.x,  0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zRot.y, -1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zRot.z,  0.0f, ST::EPSILON);

    // rotationY(90deg): z -> x, x -> -z
    Matrix4x4 Ry90 = Matrix4x4::rotationY(ST::degreesToRadians(90.0f));
    Vector3 yZ = Ry90.transformDirection(Vector3(0, 0, 1));
    EXPECT_NEAR(ctx, yZ.x,  1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yZ.y,  0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yZ.z,  0.0f, ST::EPSILON);
    Vector3 yX = Ry90.transformDirection(Vector3(1, 0, 0));
    EXPECT_NEAR(ctx, yX.x,  0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yX.y,  0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, yX.z, -1.0f, ST::EPSILON);

    // rotationZ(90deg): x -> y, y -> -x
    Matrix4x4 Rz90 = Matrix4x4::rotationZ(ST::degreesToRadians(90.0f));
    Vector3 zX = Rz90.transformDirection(Vector3(1, 0, 0));
    EXPECT_NEAR(ctx, zX.x, 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zX.y, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zX.z, 0.0f, ST::EPSILON);
    Vector3 zY = Rz90.transformDirection(Vector3(0, 1, 0));
    EXPECT_NEAR(ctx, zY.x, -1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zY.y,  0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, zY.z,  0.0f, ST::EPSILON);

    // rotationZ(360deg) should equal identity (transcendentals -- use slop).
    Matrix4x4 RzFull = Matrix4x4::rotationZ(ST::degreesToRadians(360.0f));
    EXPECT_TRUE(ctx, matrixNear(RzFull, Matrix4x4::identity(), 1e-4f));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// =============================================================
// Projection / view matrices
// =============================================================
inline void test_Matrix4x4_Perspective(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;

    Matrix4x4 P = Matrix4x4::perspective(
        ST::degreesToRadians(90.0f), 1.0f, 1.0f, 100.0f);

    // Standard OpenGL/GLM-style perspective has:
    //   m[3][2] = -1  (and m[3][3] = 0 to mark homogeneous-divide by -z).
    EXPECT_NEAR(ctx, P.m[3][2], -1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, P.m[3][3],  0.0f, ST::EPSILON);

    // A point on the near plane should map to z' close to -1 in NDC.
    // (z' = (m[2][2]*z + m[2][3]) / -z  with z=1  ->  (m[2][2]*1 + m[2][3]) / -1)
    ST::Vector4 nearPt(0, 0, -1.0f, 1.0f); // -z because camera looks down -z
    ST::Vector4 ndcNear = P * nearPt;
    EXPECT_NEAR(ctx, ndcNear.z / ndcNear.w, -1.0f, 1e-3f);

    // A point on the far plane should map to z' close to +1.
    ST::Vector4 farPt(0, 0, -100.0f, 1.0f);
    ST::Vector4 ndcFar = P * farPt;
    EXPECT_NEAR(ctx, ndcFar.z / ndcFar.w, 1.0f, 1e-3f);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

inline void test_Matrix4x4_LookAt(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Matrix4x4;
    using ST::Vector3;

    // Camera at (0,0,5) looking down -Z toward the origin.  Up = +Y.
    Matrix4x4 V = Matrix4x4::lookAt(
        Vector3(0, 0, 5),
        Vector3(0, 0, 0),
        Vector3(0, 1, 0)
    );

    // The eye itself should map to the origin in view space.
    Vector3 eyeInView = V.transformPoint(Vector3(0, 0, 5));
    EXPECT_NEAR(ctx, eyeInView.x,  0.0f, 1e-4f);
    EXPECT_NEAR(ctx, eyeInView.y,  0.0f, 1e-4f);
    EXPECT_NEAR(ctx, eyeInView.z,  0.0f, 1e-4f);

    // The target should map to (0,0,-5) in view space (in front of camera).
    // With lookAt implementation using z = (eye - target).normalized(),
    // target lies 5 units along +z' (camera-forward axis is +z after lookAt
    // here, depending on convention).  This is exactly the contract we want
    // to lock in: target lands at the same axis-distance as |eye - target|.
    Vector3 targetInView = V.transformPoint(Vector3(0, 0, 0));
    EXPECT_NEAR(ctx, std::fabs(targetInView.z), 5.0f, 1e-4f);

    // World up direction must remain orthogonal to world forward
    // (no accidental tilt).
    Vector3 upDir = V.transformDirection(Vector3(0, 1, 0));
    Vector3 fwdDir = V.transformDirection(Vector3(0, 0, -1));
    EXPECT_NEAR(ctx, upDir.dot(fwdDir), 0.0f, 1e-4f);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

} // namespace review::math
