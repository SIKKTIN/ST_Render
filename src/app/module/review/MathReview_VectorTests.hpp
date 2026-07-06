#pragma once

#include "MathReview_Harness.hpp"
#include "core/math/Vector2.hpp"
#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"

namespace review::math {

// =============================================================
// Vector2
// =============================================================
inline void test_Vector2_Basics(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Vector2;

    Vector2 a(3.0f, 4.0f);
    Vector2 b(1.0f, 2.0f);

    // Arithmetic
    EXPECT_TRUE(ctx, (a + b) == Vector2(4.0f, 6.0f));
    EXPECT_TRUE(ctx, (a - b) == Vector2(2.0f, 2.0f));
    EXPECT_TRUE(ctx, (a * 2.0f) == Vector2(6.0f, 8.0f));
    EXPECT_TRUE(ctx, (2.0f * a) == Vector2(6.0f, 8.0f));
    EXPECT_TRUE(ctx, (a / 2.0f) == Vector2(1.5f, 2.0f));
    // Vector2 has no unary minus (only Vector3 does).  Use scalar(0) - a
    // to keep this test focused on the rest of the arithmetic surface.
    EXPECT_TRUE(ctx, (Vector2(0, 0) - a) == Vector2(-3.0f, -4.0f));

    // dot / length / normalize
    EXPECT_NEAR(ctx, a.dot(b), 11.0f, ST::EPSILON); // 3*1 + 4*2
    EXPECT_NEAR(ctx, a.length(), 5.0f, ST::EPSILON); // (3,4) classic 3-4-5
    EXPECT_NEAR(ctx, a.lengthSquared(), 25.0f, ST::EPSILON);

    Vector2 n = a.normalized();
    EXPECT_NEAR(ctx, n.x, 0.6f, ST::EPSILON);
    EXPECT_NEAR(ctx, n.y, 0.8f, ST::EPSILON);
    EXPECT_NEAR(ctx, n.length(), 1.0f, ST::EPSILON);

    // Zero-length normalize must not crash and must return zero (the lib's
    // safety behavior).  If a future change returns (NaN,NaN) here, that
    // would silently corrupt downstream code, so we lock the contract.
    Vector2 z = Vector2::zero().normalized();
    EXPECT_TRUE(ctx, z == Vector2(0.0f, 0.0f));

    // Compound assignment
    Vector2 c = a; c += b;
    EXPECT_TRUE(ctx, c == Vector2(4.0f, 6.0f));
    c = a; c -= b;
    EXPECT_TRUE(ctx, c == Vector2(2.0f, 2.0f));
    c = a; c *= 2.0f;
    EXPECT_TRUE(ctx, c == Vector2(6.0f, 8.0f));
    c = a; c /= 2.0f;
    EXPECT_TRUE(ctx, c == Vector2(1.5f, 2.0f));

    // Static factories
    EXPECT_TRUE(ctx, Vector2::zero()  == Vector2(0, 0));
    EXPECT_TRUE(ctx, Vector2::one()   == Vector2(1, 1));
    EXPECT_TRUE(ctx, Vector2::right() == Vector2(1, 0));
    EXPECT_TRUE(ctx, Vector2::up()    == Vector2(0, 1));

    // lerp endpoints + midpoint
    EXPECT_TRUE(ctx, Vector2::lerp(a, b, 0.0f) == a);
    EXPECT_TRUE(ctx, Vector2::lerp(a, b, 1.0f) == b);
    EXPECT_TRUE(ctx, Vector2::lerp(a, b, 0.5f) == Vector2(2.0f, 3.0f));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// =============================================================
// Vector3
// =============================================================
inline void test_Vector3_Basics(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Vector3;

    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);

    // Arithmetic
    EXPECT_TRUE(ctx, (a + b) == Vector3(5, 7, 9));
    EXPECT_TRUE(ctx, (a - b) == Vector3(-3, -3, -3));
    EXPECT_TRUE(ctx, (a * 2.0f) == Vector3(2, 4, 6));
    EXPECT_TRUE(ctx, (2.0f * a) == Vector3(2, 4, 6));
    EXPECT_TRUE(ctx, (a * b)   == Vector3(4, 10, 18));     // componentwise
    EXPECT_TRUE(ctx, (a / 2.0f) == Vector3(0.5f, 1.0f, 1.5f));
    EXPECT_TRUE(ctx, (-a) == Vector3(-1, -2, -3));

    // dot = 1*4 + 2*5 + 3*6 = 32
    EXPECT_NEAR(ctx, a.dot(b), 32.0f, ST::EPSILON);

    // cross: (1,2,3) x (4,5,6) = (-3, 6, -3)
    Vector3 c = a.cross(b);
    EXPECT_TRUE(ctx, c == Vector3(-3.0f, 6.0f, -3.0f));

    // Anti-commutativity: b x a = -(a x b)
    EXPECT_TRUE(ctx, b.cross(a) == -c);

    // Cross is perpendicular to both operands.
    EXPECT_NEAR(ctx, c.dot(a), 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, c.dot(b), 0.0f, ST::EPSILON);

    // Cross of parallel vectors is zero.
    Vector3 p = Vector3(2.0f, 4.0f, 6.0f);
    EXPECT_TRUE(ctx, a.cross(p) == Vector3(0, 0, 0));

    // length / normalize
    // (2,3,6) has length 7
    Vector3 v(2.0f, 3.0f, 6.0f);
    EXPECT_NEAR(ctx, v.length(), 7.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, v.lengthSquared(), 49.0f, ST::EPSILON);

    Vector3 n = v.normalized();
    EXPECT_NEAR(ctx, n.x, 2.0f / 7.0f, 1e-5f);
    EXPECT_NEAR(ctx, n.y, 3.0f / 7.0f, 1e-5f);
    EXPECT_NEAR(ctx, n.z, 6.0f / 7.0f, 1e-5f);
    EXPECT_NEAR(ctx, n.length(), 1.0f, 1e-5f);

    // normalize() in place
    Vector3 n2 = v;
    n2.normalize();
    EXPECT_TRUE(ctx, n == n2);

    // Zero-length normalize safety
    Vector3 z = Vector3::zero().normalized();
    EXPECT_TRUE(ctx, z == Vector3(0, 0, 0));

    // Static factories
    EXPECT_TRUE(ctx, Vector3::zero()    == Vector3(0, 0, 0));
    EXPECT_TRUE(ctx, Vector3::up()      == Vector3(0, 1, 0));
    EXPECT_TRUE(ctx, Vector3::right()   == Vector3(1, 0, 0));
    EXPECT_TRUE(ctx, Vector3::forward() == Vector3(0, 0, 1));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// =============================================================
// Vector4
// =============================================================
inline void test_Vector4_Basics(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Vector3;
    using ST::Vector4;

    Vector4 a(1, 2, 3, 4);
    Vector4 b(5, 6, 7, 8);

    // Arithmetic
    EXPECT_TRUE(ctx, (a + b) == Vector4(6, 8, 10, 12));
    EXPECT_TRUE(ctx, (a - b) == Vector4(-4, -4, -4, -4));
    EXPECT_TRUE(ctx, (a * 2.0f) == Vector4(2, 4, 6, 8));

    // dot = 1*5+2*6+3*7+4*8 = 70
    EXPECT_NEAR(ctx, a.dot(b), 70.0f, ST::EPSILON);

    // Equality
    EXPECT_TRUE(ctx, a == Vector4(1, 2, 3, 4));
    EXPECT_TRUE(ctx, a != b);
    EXPECT_TRUE(ctx, Vector4::zero() == Vector4(0, 0, 0, 0));
    EXPECT_TRUE(ctx, Vector4::one()  == Vector4(1, 1, 1, 1));

    // toVector3 / toVector3Perspective
    Vector3 flat = a.toVector3();
    EXPECT_TRUE(ctx, flat == Vector3(1, 2, 3));
    EXPECT_NEAR(ctx, flat.length(), std::sqrt(14.0f), 1e-5f);

    // Perspective divide: (2,4,6,2) -> (1,2,3)
    Vector4 ph(2.0f, 4.0f, 6.0f, 2.0f);
    Vector3 pp = ph.toVector3Perspective();
    EXPECT_TRUE(ctx, pp == Vector3(1, 2, 3));

    // w==0 must NOT divide (avoids inf/NaN).  Vector4's policy: fall back
    // to dropping w only.  Lock that contract.
    Vector4 w0(1, 2, 3, 0);
    Vector3 pd = w0.toVector3Perspective();
    EXPECT_TRUE(ctx, pd == Vector3(1, 2, 3));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

} // namespace review::math
