#pragma once

// Lightweight test harness for the Math review module.
//
// Why not a full framework?  This module is the only consumer of the harness
// and the goal is "verify correctness of core math types so we can rule them
// out when debugging the 3D pipeline".  A handful of macros plus an assertion
// counter are plenty.
//
// Each test case is a plain `void run()` that mutates a shared `ReviewCtx`
// (counters + log).  The top-level module iterates them and prints
// "PASS"/"FAIL" lines to the console pane.

#include "core/math/MathUtils.hpp"
#include <cstdio>
#include <cmath>
#include <string>
#include <sstream>

namespace review::math {

struct ReviewCtx {
    int  passed   = 0;
    int  failed   = 0;
    int  checks   = 0;     // total individual EXPECT calls
    std::ostringstream log;

    // Scratch flag set by EXPECT macros if a sub-check fails. The top-level
    // test function reads this AFTER running so a single failing EXPECT inside
    // the test still marks the whole test as failed without aborting.
    bool currentFailed = false;

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
};

// ---- EXPECT macros --------------------------------------------------------
// Usage inside a test:
//     EXPECT_TRUE(ctx, v.length() > 0);
//     EXPECT_NEAR(ctx, v.x, 3.0f, 1e-5f);

#define EXPECT_TRUE(ctx, expr) \
    (ctx).expectTrue((expr), #expr, __FILE__, __LINE__)

#define EXPECT_NEAR(ctx, a, b, tol) \
    (ctx).expectNear((a), (b), (tol), #a, #b, __FILE__, __LINE__)

// "Exact" equality for floats uses MathUtils::EPSILON (1e-6f).  Use this for
// places where the correct answer is provably exact (integer-scale vectors,
// dot/cross with perpendicular axes, etc.) and EXPECT_NEAR only when a
// reference implementation involves transcendentals.
#define EXPECT_EQ_FLOAT(ctx, a, b) \
    EXPECT_NEAR((ctx), (a), (b), ST::EPSILON)

} // namespace review::math
