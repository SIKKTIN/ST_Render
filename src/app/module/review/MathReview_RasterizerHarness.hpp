#pragma once

// Lightweight test harness for the Rasterizer review module.
//
// Extends the math-review harness with rasterizer-specific helpers:
//   - A small in-memory FrameBuffer + DepthBuffer for pixel-level assertions
//   - Helpers to count how many pixels of each colour landed where
//   - A Snapshot struct to record pixel state for later comparison
//
// All tests run headless: no GPU, no SDL -- everything fits in a
// std::vector on the heap.

#include "core/math/MathUtils.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/Rasterizer.hpp"

#include <cstdio>
#include <cmath>
#include <string>
#include <sstream>
#include <vector>
#include <functional>
#include <algorithm>
#include <set>

namespace review::rasterizer {

// ---------------------------------------------------------------------------
// RasterizerCtx: the shared "world" each test mutates.
// Mirrors the Math-review ReviewCtx pattern: pass it into every test,
// accumulate pass/fail in it, then check ctx.failed at the end.
// ---------------------------------------------------------------------------
struct RasterizerCtx {
    int  passed = 0;
    int  failed = 0;
    int  checks = 0;
    std::ostringstream log;

    bool currentFailed = false;

    // The buffers and rasterizer under test.  Tests set these up and
    // then call rasterizeTriangle to fill them.
    ST::FrameBuffer*  fb   = nullptr;
    ST::DepthBuffer*  db   = nullptr;
    ST::Rasterizer*   rast = nullptr;

    void expectTrue(bool cond, const char* expr, const char* file, int line) {
        ++checks;
        if (!cond) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  expected true: " << expr << "\n";
        }
    }

    void expectNear(float a, float b, float tol, const char* ax, const char* bx,
                    const char* file, int line) {
        ++checks;
        if (!(std::fabs(a - b) <= tol)) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  |" << ax << " - " << bx
                << "| = " << std::fabs(a - b)
                << "  > tol " << tol
                << "  (got " << a << ", expected " << b << ")\n";
        }
    }

    // Convenience: pass a colour and assert the pixel matches.
    // x/y are in screen-space pixel coords (origin top-left).
    void expectPixel(int x, int y, const ST::Color& expected,
                     const char* file, int line) {
        ++checks;
        if (!fb) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  expectPixel called but fb==nullptr\n";
            return;
        }
        ST::Color got = fb->getPixel(x, y);
        bool ok = (got.r == expected.r && got.g == expected.g &&
                   got.b == expected.b && got.a == expected.a);
        if (!ok) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  pixel(" << x << "," << y << ")  "
                << "expected (" << expected.r << "," << expected.g
                << "," << expected.b << "," << expected.a << ")  "
                << "got (" << got.r << "," << got.g
                << "," << got.b << "," << got.a << ")\n";
        }
    }

    // Assert that a pixel is exactly black (no triangle coverage).
    void expectBlackPixel(int x, int y, const char* file, int line) {
        expectPixel(x, y, ST::Color::black(), file, line);
    }

    // Assert that a pixel is NOT black (some triangle painted it).
    void expectNonBlackPixel(int x, int y, const char* file, int line) {
        ++checks;
        if (!fb) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  expectNonBlackPixel called but fb==nullptr\n";
            return;
        }
        ST::Color got = fb->getPixel(x, y);
        bool ok = (got.r > 0.0f || got.g > 0.0f || got.b > 0.0f);
        if (!ok) {
            currentFailed = true;
            log << "    [FAIL] " << file << ":" << line
                << "  pixel(" << x << "," << y << ")  "
                << "expected non-black, got black\n";
        }
    }

    // Count pixels of a given colour (exact match) within a rectangular region.
    int countPixelsInRect(int x0, int y0, int w, int h,
                           const ST::Color& target) const {
        int count = 0;
        for (int y = y0; y < y0 + h; ++y) {
            for (int x = x0; x < x0 + w; ++x) {
                ST::Color p = fb->getPixel(x, y);
                if (p.r == target.r && p.g == target.g &&
                    p.b == target.b && p.a == target.a)
                    ++count;
            }
        }
        return count;
    }

    // Count pixels that are NOT black within a rectangular region.
    int countNonBlackPixelsInRect(int x0, int y0, int w, int h) const {
        int count = 0;
        for (int y = y0; y < y0 + h; ++y) {
            for (int x = x0; x < x0 + w; ++x) {
                ST::Color p = fb->getPixel(x, y);
                if (p.r > 0.0f || p.g > 0.0f || p.b > 0.0f) ++count;
            }
        }
        return count;
    }
};

// ---- EXPECT macros --------------------------------------------------------
// Usage inside a rasterizer test:
//     EXPECT_TRUE(ctx, v.length() > 0);
//     EXPECT_NEAR(ctx, v.x, 3.0f, 1e-5f);
//     EXPECT_PIXEL(ctx, 5, 3, ST::Color::red());
//     EXPECT_BLACK_PIXEL(ctx, 0, 0);

#define EXPECT_TRUE(ctx, expr) \
    (ctx).expectTrue((expr), #expr, __FILE__, __LINE__)

#define EXPECT_NEAR(ctx, a, b, tol) \
    (ctx).expectNear((a), (b), (tol), #a, #b, __FILE__, __LINE__)

#define EXPECT_PIXEL(ctx, x, y, color) \
    (ctx).expectPixel((x), (y), (color), __FILE__, __LINE__)

#define EXPECT_BLACK_PIXEL(ctx, x, y) \
    (ctx).expectBlackPixel((x), (y), __FILE__, __LINE__)

#define EXPECT_NONBLACK_PIXEL(ctx, x, y) \
    (ctx).expectNonBlackPixel((x), (y), __FILE__, __LINE__)

// "Exact" equality for floats uses MathUtils::EPSILON (1e-6f).
#define EXPECT_EQ_FLOAT(ctx, a, b) \
    EXPECT_NEAR((ctx), (a), (b), ST::EPSILON)

// ---- Setup helpers --------------------------------------------------------
//
// These are free functions (not macros) so they can be called cleanly from
// within lambdas.  They configure the RasterizerCtx with a freshly
// allocated in-memory FrameBuffer + DepthBuffer of the given size.
// Tests call setup(ctx, w, h) at the start and then call rasterizeTriangle()
// directly on ctx.rast.
//
// setup() returns true on success (allocation succeeded).
inline bool setupBuffers(RasterizerCtx& ctx, int w, int h) {
    // Free previous if any (tests that reuse the ctx variable in a loop).
    delete ctx.db;
    delete ctx.fb;
    delete ctx.rast;

    ctx.fb   = new ST::FrameBuffer();
    ctx.db   = new ST::DepthBuffer();
    ctx.rast = new ST::Rasterizer();

    ctx.fb->initialize(w, h);
    ctx.db->initialize(w, h);
    ctx.rast->setBuffers(ctx.fb, ctx.db);
    ctx.rast->setUseRawScreenCoords(true);
    return true;
}

inline void teardownBuffers(RasterizerCtx& ctx) {
    delete ctx.db;   ctx.db   = nullptr;
    delete ctx.fb;    ctx.fb   = nullptr;
    delete ctx.rast;  ctx.rast = nullptr;
}

// ---------------------------------------------------------------------------
// PixelCollector: utility to collect all non-black pixel positions from a
// framebuffer.  Useful for quick visual sanity checks without enumerating
// every pixel individually.
// ---------------------------------------------------------------------------
struct PixelCollector {
    std::vector<std::pair<int, int>> positions;  // {x, y}

    explicit PixelCollector(const ST::FrameBuffer* fb,
                            int x0 = 0, int y0 = 0,
                            int w = -1, int h = -1) {
        int W = fb->getWidth();
        int H = fb->getHeight();
        if (w < 0) w = W;
        if (h < 0) h = H;
        int x1 = std::min(x0 + w, W);
        int y1 = std::min(y0 + h, H);
        for (int y = std::max(y0, 0); y < y1; ++y) {
            for (int x = std::max(x0, 0); x < x1; ++x) {
                ST::Color p = fb->getPixel(x, y);
                if (p.r > 0.0f || p.g > 0.0f || p.b > 0.0f) {
                    positions.emplace_back(x, y);
                }
            }
        }
    }

    int count() const { return (int)positions.size(); }
    bool empty() const { return positions.empty(); }
};

} // namespace review::rasterizer
