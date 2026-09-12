#pragma once

// Rasterizer review tests.
//
// Each function is a self-contained correctness check on a single
// rasterizer primitive (barycentric, interpolators, rasterization rules,
// depth buffer, viewport, etc.).  All run headless against an in-memory
// FrameBuffer + DepthBuffer.

#include "MathReview_RasterizerHarness.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/geometry/Vertex.hpp"

#include <cmath>
#include <algorithm>

namespace review::rasterizer {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// No-op fragment shader that always returns a fixed colour.
static ST::Color solidFrag(const ST::VertexOut&, ST::Color base) {
    return base;
}

// Call rasterizeTriangle with a solid-colour fragment shader.
static void rasterizeSolid(RasterizerCtx& ctx,
                           const ST::VertexOut& v0,
                           const ST::VertexOut& v1,
                           const ST::VertexOut& v2,
                           ST::Color solid) {
    ctx.rast->rasterizeTriangle(v0, v1, v2,
        [&](const ST::VertexOut& frag) {
            return solidFrag(frag, solid);
        });
}

// ---------------------------------------------------------------------------
// Barycentric
// ---------------------------------------------------------------------------

// Pixels at triangle corners must be inside (all barycentric components >= 0).
// Pixels at triangle centroid should also be inside.
inline void test_Barycentric_CornersAndCenter(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    // Triangle: (0,0)-(10,0)-(0,10)  right-angle at origin.
    ST::Vector2 a(0.0f, 0.0f);
    ST::Vector2 b(10.0f, 0.0f);
    ST::Vector2 c(0.0f, 10.0f);

    // Corners -- all should be inside.
    ST::Vector3 baryA = ST::Rasterizer::computeBarycentric(a, a, b, c);
    ST::Vector3 baryB = ST::Rasterizer::computeBarycentric(b, a, b, c);
    ST::Vector3 baryC = ST::Rasterizer::computeBarycentric(c, a, b, c);

    EXPECT_TRUE(ctx, baryA.x >= 0.0f && baryA.y >= 0.0f && baryA.z >= 0.0f);
    EXPECT_TRUE(ctx, baryB.x >= 0.0f && baryB.y >= 0.0f && baryB.z >= 0.0f);
    EXPECT_TRUE(ctx, baryC.x >= 0.0f && baryC.y >= 0.0f && baryC.z >= 0.0f);

    // Sum must equal 1.
    EXPECT_NEAR(ctx, baryA.x + baryA.y + baryA.z, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, baryB.x + baryB.y + baryB.z, 1.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, baryC.x + baryC.y + baryC.z, 1.0f, ST::EPSILON);

    // Centroid (average of three vertices).
    ST::Vector2 centroid(10.0f / 3.0f, 10.0f / 3.0f);
    ST::Vector3 baryG = ST::Rasterizer::computeBarycentric(centroid, a, b, c);
    EXPECT_NEAR(ctx, baryG.x + baryG.y + baryG.z, 1.0f, ST::EPSILON);
    EXPECT_TRUE(ctx, baryG.x >= -ST::EPSILON && baryG.y >= -ST::EPSILON && baryG.z >= -ST::EPSILON);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// Points on each edge must be inside (all components >= 0, at least one = 0).
// Points outside must be rejected (at least one component < 0).
inline void test_Barycentric_EdgeAndOutside(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    ST::Vector2 a(0.0f, 0.0f);
    ST::Vector2 b(10.0f, 0.0f);
    ST::Vector2 c(0.0f, 10.0f);

    // Midpoint of each edge -- exactly on the edge => one barycentric coord = 0.
    ST::Vector3 eAB = ST::Rasterizer::computeBarycentric(
        ST::Vector2(5.0f, 0.0f), a, b, c);
    ST::Vector3 eBC = ST::Rasterizer::computeBarycentric(
        ST::Vector2(5.0f, 5.0f), a, b, c);
    ST::Vector3 eCA = ST::Rasterizer::computeBarycentric(
        ST::Vector2(0.0f, 5.0f), a, b, c);

    EXPECT_TRUE(ctx, eAB.x >= -ST::EPSILON && eAB.y >= -ST::EPSILON && eAB.z >= -ST::EPSILON);
    EXPECT_TRUE(ctx, eBC.x >= -ST::EPSILON && eBC.y >= -ST::EPSILON && eBC.z >= -ST::EPSILON);
    EXPECT_TRUE(ctx, eCA.x >= -ST::EPSILON && eCA.y >= -ST::EPSILON && eCA.z >= -ST::EPSILON);

    // Points clearly outside the triangle.
    ST::Vector3 out1 = ST::Rasterizer::computeBarycentric(
        ST::Vector2(11.0f, 0.0f), a, b, c);   // past B along AB
    ST::Vector3 out2 = ST::Rasterizer::computeBarycentric(
        ST::Vector2(-1.0f, 5.0f), a, b, c);   // left of AC
    ST::Vector3 out3 = ST::Rasterizer::computeBarycentric(
        ST::Vector2(6.0f, 6.0f), a, b, c);    // outside hypotenuse

    bool anyOut1 = (out1.x < -ST::EPSILON) || (out1.y < -ST::EPSILON) || (out1.z < -ST::EPSILON);
    bool anyOut2 = (out2.x < -ST::EPSILON) || (out2.y < -ST::EPSILON) || (out2.z < -ST::EPSILON);
    bool anyOut3 = (out3.x < -ST::EPSILON) || (out3.y < -ST::EPSILON) || (out3.z < -ST::EPSILON);

    EXPECT_TRUE(ctx, anyOut1);
    EXPECT_TRUE(ctx, anyOut2);
    EXPECT_TRUE(ctx, anyOut3);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// Barycentric coordinates of a point are the correct linear blend weights:
// alpha * A + beta * B + gamma * C == P.
inline void test_Barycentric_WeightProperty(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    ST::Vector2 a(0.0f, 0.0f);
    ST::Vector2 b(20.0f, 0.0f);
    ST::Vector2 c(0.0f, 20.0f);

    auto verifyBlend = [&](const ST::Vector2& p) {
        ST::Vector3 b_ = ST::Rasterizer::computeBarycentric(p, a, b, c);
        float rx = b_.x * a.x + b_.y * b.x + b_.z * c.x;
        float ry = b_.x * a.y + b_.y * b.y + b_.z * c.y;
        EXPECT_NEAR(ctx, rx, p.x, ST::EPSILON);
        EXPECT_NEAR(ctx, ry, p.y, ST::EPSILON);
    };

    verifyBlend(ST::Vector2(5.0f, 5.0f));   // interior
    verifyBlend(ST::Vector2(10.0f, 0.0f));  // on AB
    verifyBlend(ST::Vector2(0.0f, 0.0f));   // vertex A
    verifyBlend(ST::Vector2(10.0f, 10.0f)); // centroid-ish

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// ---------------------------------------------------------------------------
// Interpolators
// ---------------------------------------------------------------------------

inline void test_LinearInterpolate(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    ST::Vector3 bary(0.2f, 0.3f, 0.5f);
    float a0 = 10.0f, a1 = 20.0f, a2 = 30.0f;
    float result = ST::Rasterizer::linearInterpolate<float>(bary, a0, a1, a2);
    EXPECT_NEAR(ctx, result, 0.2f * 10.0f + 0.3f * 20.0f + 0.5f * 30.0f, ST::EPSILON);

    // Endpoints: bary = (1,0,0)
    bary = ST::Vector3(1.0f, 0.0f, 0.0f);
    EXPECT_NEAR(ctx, ST::Rasterizer::linearInterpolate<float>(bary, a0, a1, a2), a0, ST::EPSILON);

    // Endpoints: bary = (0,0,1)
    bary = ST::Vector3(0.0f, 0.0f, 1.0f);
    EXPECT_NEAR(ctx, ST::Rasterizer::linearInterpolate<float>(bary, a0, a1, a2), a2, ST::EPSILON);

    // Color interpolation (ST::Color supports +, *, so it works too).
    ST::Color c0(0.0f, 0.0f, 0.0f, 1.0f);
    ST::Color c1(1.0f, 0.0f, 0.0f, 1.0f);
    ST::Color c2(0.0f, 1.0f, 0.0f, 1.0f);
    ST::Vector3 bHalf(0.5f, 0.5f, 0.0f);
    ST::Color cBlend = ST::Rasterizer::linearInterpolate<ST::Color>(bHalf, c0, c1, c2);
    EXPECT_NEAR(ctx, cBlend.r, 0.5f, ST::EPSILON);
    EXPECT_NEAR(ctx, cBlend.g, 0.0f, ST::EPSILON);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// perspectiveCorrect must equal linearInterpolate when all w values are equal.
inline void test_PerspectiveCorrect_UniformW(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    ST::Vector3 bary(0.25f, 0.25f, 0.5f);
    float v0 = 1.0f, v1 = 2.0f, v2 = 4.0f;

    float linear = ST::Rasterizer::linearInterpolate<float>(bary, v0, v1, v2);
    float correct = ST::Rasterizer::perspectiveCorrect<float>(bary, v0, v1, v2, 1.0f, 1.0f, 1.0f);

    EXPECT_NEAR(ctx, correct, linear, ST::EPSILON);

    // Also test with arbitrary but equal w values.
    correct = ST::Rasterizer::perspectiveCorrect<float>(bary, v0, v1, v2, 5.0f, 5.0f, 5.0f);
    EXPECT_NEAR(ctx, correct, linear, ST::EPSILON);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// With varying w, perspectiveCorrect gives different (correct) results from linear.
inline void test_PerspectiveCorrect_VaryingW(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    // Near fragment (small w) should pull the interpolated value toward v0.
    // Far fragment (large w) should pull toward v2.
    ST::Vector3 bary(0.5f, 0.0f, 0.5f);
    float v0 = 100.0f;  // near (small w)
    float v1 = 0.0f;
    float v2 = 0.0f;    // far (large w)
    float w0 = 1.0f;    // small w  => near vertex
    float w1 = 1.0f;
    float w2 = 10.0f;   // large w => far vertex

    float pc = ST::Rasterizer::perspectiveCorrect<float>(bary, v0, v1, v2, w0, w1, w2);

    // Linear would give 50.0.  Perspective-correct with w0=1 and w2=10,
    // the near contribution is weighted up, so result should be > 50.
    float linear = ST::Rasterizer::linearInterpolate<float>(bary, v0, v1, v2);
    EXPECT_TRUE(ctx, pc > linear);

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// ---------------------------------------------------------------------------
// Rasterization -- triangle fills
// ---------------------------------------------------------------------------

inline void test_Rasterize_SolidTriangle(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    // Right-angle triangle at origin, spanning roughly 8x8 pixels.
    ST::VertexOut v0; v0.position = ST::Vector4(0.0f, 0.0f, 0.0f, 1.0f); v0.color = ST::Color::blue();
    ST::VertexOut v1; v1.position = ST::Vector4(8.0f, 0.0f, 0.0f, 1.0f); v1.color = ST::Color::blue();
    ST::VertexOut v2; v2.position = ST::Vector4(0.0f, 8.0f, 0.0f, 1.0f); v2.color = ST::Color::blue();

    rasterizeSolid(ctx, v0, v1, v2, ST::Color::blue());

    // Corners of the triangle bounding box must be painted.
    // Note: exact pixel coverage depends on rasterizer's bounding-box clamp
    // and EPSILON in barycentric check.  We test a few representative pixels.
    int filled = ctx.countNonBlackPixelsInRect(0, 0, 9, 9);
    EXPECT_TRUE(ctx, filled > 0);   // at least something was drawn

    // Pixels clearly outside (e.g. (9,0) is on the AB edge) should be non-black.
    EXPECT_NONBLACK_PIXEL(ctx, 3, 1);
    EXPECT_NONBLACK_PIXEL(ctx, 1, 3);
    EXPECT_NONBLACK_PIXEL(ctx, 2, 2);

    // Pixels clearly outside the triangle must remain black.
    EXPECT_BLACK_PIXEL(ctx, 8, 5);  // outside hypotenuse
    EXPECT_BLACK_PIXEL(ctx, 9, 0);  // past the right-angle corner

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// A tiny 1x1 "pixel" triangle should fill exactly one pixel.
inline void test_Rasterize_TinyOnePixelTriangle(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    // All three vertices snap to the same pixel.
    ST::VertexOut v0; v0.position = ST::Vector4(3.0f, 3.0f, 0.0f, 1.0f); v0.color = ST::Color::red();
    ST::VertexOut v1; v1.position = ST::Vector4(3.0f, 3.0f, 0.0f, 1.0f); v1.color = ST::Color::red();
    ST::VertexOut v2; v2.position = ST::Vector4(3.0f, 3.0f, 0.0f, 1.0f); v2.color = ST::Color::red();

    rasterizeSolid(ctx, v0, v1, v2, ST::Color::red());

    EXPECT_NONBLACK_PIXEL(ctx, 3, 3);

    // No other pixels in the 5x5 neighbourhood should be painted.
    bool anyOther = false;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int x = 3 + dx, y = 3 + dy;
            if (x < 0 || x >= 10 || y < 0 || y >= 10) continue;
            ST::Color p = ctx.fb->getPixel(x, y);
            if (p.r > 0.0f || p.g > 0.0f || p.b > 0.0f) {
                anyOther = true;
            }
        }
    }
    EXPECT_TRUE(ctx, !anyOther);

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// ---------------------------------------------------------------------------
// Rasterization -- colour interpolation
// ---------------------------------------------------------------------------

// Vertex colours are linearly interpolated across the triangle.
inline void test_Rasterize_ColorInterpolation(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    // Red at (0,0), Green at (0,0), Blue at (0,0) -- each vertex a different colour.
    // Use a degenerate approach: all at same pixel, then test midpoints.
    // Instead, use a spread-out triangle and check that colours vary.

    // Triangle spans (0,0) red, (9,0) green, (0,9) blue.
    ST::VertexOut v0; v0.position = ST::Vector4(0.0f, 0.0f, 0.0f, 1.0f); v0.color = ST::Color::red();
    ST::VertexOut v1; v1.position = ST::Vector4(9.0f, 0.0f, 0.0f, 1.0f); v1.color = ST::Color::green();
    ST::VertexOut v2; v2.position = ST::Vector4(0.0f, 9.0f, 0.0f, 1.0f); v2.color = ST::Color::blue();

    ctx.rast->rasterizeTriangle(v0, v1, v2,
        [&](const ST::VertexOut& frag) {
            return frag.color;
        });

    // Pixel at v0 (0,0) should be red.
    ST::Color p00 = ctx.fb->getPixel(0, 0);
    EXPECT_TRUE(ctx, p00.r > p00.g && p00.r > p00.b);

    // Pixel at v1 (9,0) should be green.
    ST::Color p90 = ctx.fb->getPixel(9, 0);
    EXPECT_TRUE(ctx, p90.g > p90.r && p90.g > p90.b);

    // Pixel at v2 (0,9) should be blue.
    ST::Color p09 = ctx.fb->getPixel(0, 9);
    EXPECT_TRUE(ctx, p09.b > p09.r && p09.b > p09.g);

    // Centroid-ish (3,3) should have non-zero values in all channels.
    ST::Color pMid = ctx.fb->getPixel(3, 3);
    EXPECT_TRUE(ctx, pMid.r > 0.0f && pMid.g > 0.0f && pMid.b > 0.0f);

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// ---------------------------------------------------------------------------
// Depth buffer
// ---------------------------------------------------------------------------

// When two triangles overlap, the closer one wins.
inline void test_DepthBuffer_OverwriteCloser(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    // First triangle at z=0.5 (closer to camera).
    ST::VertexOut f0; f0.position = ST::Vector4(2.0f, 2.0f, 0.5f, 1.0f); f0.color = ST::Color::red();
    ST::VertexOut f1; f1.position = ST::Vector4(7.0f, 2.0f, 0.5f, 1.0f); f1.color = ST::Color::red();
    ST::VertexOut f2; f2.position = ST::Vector4(2.0f, 7.0f, 0.5f, 1.0f); f2.color = ST::Color::red();
    rasterizeSolid(ctx, f0, f1, f2, ST::Color::red());

    // Second triangle at z=0.25 (even closer).
    ST::VertexOut b0; b0.position = ST::Vector4(3.0f, 3.0f, 0.25f, 1.0f); b0.color = ST::Color::green();
    ST::VertexOut b1; b1.position = ST::Vector4(8.0f, 3.0f, 0.25f, 1.0f); b1.color = ST::Color::green();
    ST::VertexOut b2; b2.position = ST::Vector4(3.0f, 8.0f, 0.25f, 1.0f); b2.color = ST::Color::green();
    rasterizeSolid(ctx, b0, b1, b2, ST::Color::green());

    // Overlap region (3,3)-(7,3)-(3,7) should be green (second, closer triangle wins).
    EXPECT_PIXEL(ctx, 4, 4, ST::Color::green());
    EXPECT_PIXEL(ctx, 5, 4, ST::Color::green());
    EXPECT_PIXEL(ctx, 4, 5, ST::Color::green());

    // Region covered only by the first triangle stays red.
    EXPECT_PIXEL(ctx, 2, 2, ST::Color::red());

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// When the second triangle is farther, the first should remain visible.
inline void test_DepthBuffer_FartherDoesNotOverwrite(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    // First triangle at z=0.25 (closer).
    ST::VertexOut c0; c0.position = ST::Vector4(2.0f, 2.0f, 0.25f, 1.0f); c0.color = ST::Color::blue();
    ST::VertexOut c1; c1.position = ST::Vector4(7.0f, 2.0f, 0.25f, 1.0f); c1.color = ST::Color::blue();
    ST::VertexOut c2; c2.position = ST::Vector4(2.0f, 7.0f, 0.25f, 1.0f); c2.color = ST::Color::blue();
    rasterizeSolid(ctx, c0, c1, c2, ST::Color::blue());

    // Second triangle at z=0.5 (farther).
    ST::VertexOut f0; f0.position = ST::Vector4(3.0f, 3.0f, 0.5f, 1.0f); f0.color = ST::Color::yellow();
    ST::VertexOut f1; f1.position = ST::Vector4(8.0f, 3.0f, 0.5f, 1.0f); f1.color = ST::Color::yellow();
    ST::VertexOut f2; f2.position = ST::Vector4(3.0f, 8.0f, 0.5f, 1.0f); f2.color = ST::Color::yellow();
    rasterizeSolid(ctx, f0, f1, f2, ST::Color::yellow());

    // Overlap region should still be blue (closer first triangle wins).
    EXPECT_PIXEL(ctx, 4, 4, ST::Color::blue());
    EXPECT_PIXEL(ctx, 5, 4, ST::Color::blue());

    // Non-overlap part of second triangle should be yellow.
    EXPECT_PIXEL(ctx, 6, 3, ST::Color::yellow());

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// testAndSet returns true on the first write and false on subsequent writes
// at the same depth.
inline void test_DepthBuffer_EqualDepth(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    // Direct DepthBuffer test (no rasterizer).
    ST::DepthBuffer db;
    db.initialize(10, 10);

    // First write at (5,5) with depth=0.5 -> succeeds.
    bool first = db.testAndSet(5, 5, 0.5f);
    EXPECT_TRUE(ctx, first);
    EXPECT_NEAR(ctx, db.getDepth(5, 5), 0.5f, ST::EPSILON);

    // Second write at same pixel, same depth -> fails (not strictly closer).
    bool second = db.testAndSet(5, 5, 0.5f);
    EXPECT_TRUE(ctx, !second);

    // Deeper write -> fails.
    bool deeper = db.testAndSet(5, 5, 0.75f);
    EXPECT_TRUE(ctx, !deeper);

    // Shallower write -> succeeds and updates.
    bool shallower = db.testAndSet(5, 5, 0.25f);
    EXPECT_TRUE(ctx, shallower);
    EXPECT_NEAR(ctx, db.getDepth(5, 5), 0.25f, ST::EPSILON);

    // Out-of-bounds should return false.
    EXPECT_TRUE(ctx, !db.testAndSet(-1, 5, 0.1f));
    EXPECT_TRUE(ctx, !db.testAndSet(5, -1, 0.1f));
    EXPECT_TRUE(ctx, !db.testAndSet(10, 5, 0.1f));  // w=10, buffer is 10 wide (indices 0-9)
    EXPECT_TRUE(ctx, !db.testAndSet(5, 10, 0.1f));  // h=10, indices 0-9

    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// ---------------------------------------------------------------------------
// Viewport / screen-space transform
// ---------------------------------------------------------------------------

// With setUseRawScreenCoords(true), NDC is ignored and raw clip-space xy is used.
inline void test_ToScreenSpace_RawCoords(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 10, 10);

    ctx.rast->setUseRawScreenCoords(true);

    ST::VertexOut v;
    v.position = ST::Vector4(3.0f, 7.0f, 0.0f, 1.0f);
    ST::Vector2 ss = ctx.rast->toScreenSpace(v);

    EXPECT_NEAR(ctx, ss.x, 3.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, ss.y, 7.0f, ST::EPSILON);

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// With setUseRawScreenCoords(false), clip xy is converted through NDC + viewport.
inline void test_ToScreenSpace_NDCViewport(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 100, 100);

    ctx.rast->setUseRawScreenCoords(false);
    ctx.rast->setViewport(0, 0, 100, 100);

    // NDC (-1,-1) -> screen (0, 100)  (y is flipped).
    ST::VertexOut v;
    v.position = ST::Vector4(-1.0f, -1.0f, 0.0f, 1.0f);
    ST::Vector2 ss = ctx.rast->toScreenSpace(v);
    EXPECT_NEAR(ctx, ss.x, 0.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, ss.y, 100.0f, ST::EPSILON);

    // NDC (+1,+1) -> screen (100, 0).
    v.position = ST::Vector4(1.0f, 1.0f, 0.0f, 1.0f);
    ss = ctx.rast->toScreenSpace(v);
    EXPECT_NEAR(ctx, ss.x, 100.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, ss.y, 0.0f, ST::EPSILON);

    // NDC (0,0) -> screen (50, 50)  (centre of 100x100 viewport).
    v.position = ST::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    ss = ctx.rast->toScreenSpace(v);
    EXPECT_NEAR(ctx, ss.x, 50.0f, ST::EPSILON);
    EXPECT_NEAR(ctx, ss.y, 50.0f, ST::EPSILON);

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

// setViewport clamps to buffer bounds.
inline void test_Viewport_Clamp(review::rasterizer::RasterizerCtx& ctx) {
    ctx.currentFailed = false;

    setupBuffers(ctx, 100, 100);

    ctx.rast->setUseRawScreenCoords(false);
    ctx.rast->setViewport(10, 10, 200, 200);  // larger than buffer, should clamp
    EXPECT_TRUE(ctx, ctx.rast->m_viewportWidth  == 90);   // 100 - 10
    EXPECT_TRUE(ctx, ctx.rast->m_viewportHeight == 90);   // 100 - 10

    ctx.rast->setViewport(-5, -5, 50, 50);    // negative offset, should clamp
    EXPECT_TRUE(ctx, ctx.rast->m_viewportX == 0);
    EXPECT_TRUE(ctx, ctx.rast->m_viewportY == 0);
    EXPECT_TRUE(ctx, ctx.rast->m_viewportWidth  == 50);
    EXPECT_TRUE(ctx, ctx.rast->m_viewportHeight == 50);

    ctx.rast->setViewport(30, 40, 0, 0);      // zero size, should clamp to min 1
    EXPECT_TRUE(ctx, ctx.rast->m_viewportWidth  == 1);
    EXPECT_TRUE(ctx, ctx.rast->m_viewportHeight == 1);

    teardownBuffers(ctx);
    if (!ctx.currentFailed) ++ctx.passed; else ++ctx.failed;
}

} // namespace review::rasterizer
