#pragma once

#include "MathReview_Harness.hpp"
#include "core/math/MathUtils.hpp"
#include "core/math/Vector4.hpp"

namespace review::math {

// =============================================================
// Color
// =============================================================
inline void test_Color_Basics(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::Color;
    using ST::Vector3;
    using ST::Vector4;

    // Construction
    Color a(0.2f, 0.4f, 0.6f, 0.8f);
    EXPECT_NEAR(ctx, a.r, 0.2f, ST::EPSILON);
    EXPECT_NEAR(ctx, a.g, 0.4f, ST::EPSILON);
    EXPECT_NEAR(ctx, a.b, 0.6f, ST::EPSILON);
    EXPECT_NEAR(ctx, a.a, 0.8f, ST::EPSILON);

    // Default alpha is 1.0
    Color def(0.5f, 0.5f, 0.5f);
    EXPECT_NEAR(ctx, def.a, 1.0f, ST::EPSILON);

    // Equality
    EXPECT_TRUE(ctx, Color(1, 0, 0) == Color(1, 0, 0));
    EXPECT_TRUE(ctx, Color(1, 0, 0) != Color(0, 1, 0));

    // Arithmetic
    Color r = Color::red();
    Color g = Color::green();
    Color sum = r + g;
    EXPECT_NEAR(ctx, sum.r, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, sum.g, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, sum.b, 0.0f, ST::EPSILON);

    Color scaled = r * 0.5f;
    EXPECT_NEAR(ctx, scaled.r, 0.5f, ST::EPSILON);

    Color premul = 0.5f * r;
    EXPECT_TRUE(ctx, premul == scaled);

    // Color += Color accumulates all four channels.
    Color acc = Color::black();
    acc += r;
    EXPECT_TRUE(ctx, acc == Color::red());

    // Named factories are pure colors with full alpha.
    EXPECT_TRUE(ctx, Color::red()    == Color(1, 0, 0, 1));
    EXPECT_TRUE(ctx, Color::green()  == Color(0, 1, 0, 1));
    EXPECT_TRUE(ctx, Color::blue()   == Color(0, 0, 1, 1));
    EXPECT_TRUE(ctx, Color::white()  == Color(1, 1, 1, 1));
    EXPECT_TRUE(ctx, Color::black()  == Color(0, 0, 0, 1));
    EXPECT_TRUE(ctx, Color::yellow() == Color(1, 1, 0, 1));
    EXPECT_TRUE(ctx, Color::cyan()   == Color(0, 1, 1, 1));
    EXPECT_TRUE(ctx, Color::magenta()== Color(1, 0, 1, 1));

    // lerp endpoints + midpoint
    Color mid = Color::lerp(Color::black(), Color::white(), 0.5f);
    EXPECT_NEAR(ctx, mid.r, 0.5f, ST::EPSILON);
    EXPECT_NEAR(ctx, mid.g, 0.5f, ST::EPSILON);
    EXPECT_NEAR(ctx, mid.b, 0.5f, ST::EPSILON);
    EXPECT_TRUE(ctx, Color::lerp(Color::black(), Color::white(), 0.0f) == Color::black());
    EXPECT_TRUE(ctx, Color::lerp(Color::black(), Color::white(), 1.0f) == Color::white());

    // Vector4 interop (used by shader output paths)
    Vector4 asV4 = Color::red();
    EXPECT_NEAR(ctx, asV4.x, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, asV4.y, 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, asV4.z, 0.0f, ST::EPSILON);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// =============================================================
// MathUtils
// =============================================================
inline void test_MathUtils(review::math::ReviewCtx& ctx) {
    ctx.currentFailed = false;
    using ST::degreesToRadians;
    using ST::radiansToDegrees;
    using ST::clamp;
    using ST::lerp;
    using ST::isZero;
    using ST::isEqual;

    // 180 degrees == pi radians
    EXPECT_NEAR(ctx, degreesToRadians(180.0f), ST::PI, 1e-5f);
    EXPECT_NEAR(ctx, degreesToRadians(0.0f),   0.0f,   1e-6f);
    EXPECT_NEAR(ctx, radiansToDegrees(ST::PI), 180.0f, 1e-5f);

    // round-trip
    EXPECT_NEAR(ctx, radiansToDegrees(degreesToRadians(57.5f)), 57.5f, 1e-5f);

    // clamp is inclusive at the boundaries
    EXPECT_NEAR(ctx, clamp(5.0f, 0.0f, 10.0f), 5.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, clamp(-1.0f, 0.0f, 10.0f), 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, clamp(99.0f, 0.0f, 10.0f), 10.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, clamp(0.0f, 0.0f, 10.0f), 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, clamp(10.0f, 0.0f, 10.0f), 10.0f, ST::EPSILON);

    // lerp endpoints + midpoint
    EXPECT_NEAR(ctx, lerp(0.0f, 10.0f, 0.0f), 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, lerp(0.0f, 10.0f, 1.0f), 10.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, lerp(0.0f, 10.0f, 0.5f), 5.0f, ST::EPSILON);

    // isZero respects the global EPSILON
    EXPECT_TRUE(ctx,  isZero(0.0f));
    EXPECT_TRUE(ctx,  isZero(ST::EPSILON * 0.5f));
    EXPECT_TRUE(ctx, !isZero(0.01f));

    // isEqual respects EPSILON
    EXPECT_TRUE(ctx,  isEqual(1.0f, 1.0f));
    EXPECT_TRUE(ctx,  isEqual(1.0f, 1.0f + ST::EPSILON * 0.5f));
    EXPECT_TRUE(ctx, !isEqual(1.0f, 1.01f));

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

} // namespace review::math
