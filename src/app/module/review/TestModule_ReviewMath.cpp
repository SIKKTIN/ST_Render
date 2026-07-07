#include "TestModule_ReviewMath.hpp"

#include "MathReview_Harness.hpp"
#include "MathReview_VectorTests.hpp"
#include "MathReview_MatrixTests.hpp"
#include "MathReview_ColorAndUtilsTests.hpp"

#include <cstdio>
#include <cmath>
#include <imgui.h>
#include <sstream>

namespace {

// ===========================================================================
// Bridging the existing ReviewCtx-style test functions (which return void
// and tally into an out-parameter) into the new bool-returning TestEntry
// shape so the runner doesn't care about test granularity.
//
// Internally: we cache the most recent per-test failure log so the ImGui
// row can show "  failed on <details>" inline if needed.  Today we just
// keep stdout-quiet; the rich per-check log goes to runConsole().
// ===========================================================================
struct CachedRun {
    bool       passed       = false;
    bool       ran          = false;
    std::string failureLog;   // empty when passed
};
static CachedRun gLastCache[64];   // upper bound matches total tests today
static int       gLastCacheSize = 0;

// runReviewTest: bridges the void(ReviewCtx&) test functions to bool.
// Writes the rich failure detail (per-EXPECT [FAIL] lines) into the
// caller-provided gLastCache[slot] so the FAIL row in the ImGui panel
// and the console pane can show what actually went wrong -- otherwise
// we'd just see "FAIL" with no idea which EXPECT tripped.
static bool runReviewTest(void (*fn)(review::math::ReviewCtx&),
                          CachedRun& cache,
                          CachedRun* slotArray,
                          int slotCount,
                          int slot) {
    review::math::ReviewCtx ctx;
    ctx.log.str(""); ctx.log.clear();
    fn(ctx);
    cache.ran    = true;
    cache.passed = (ctx.failed == 0);
    cache.failureLog = ctx.log.str();
    if (slotArray && slot >= 0 && slot < slotCount) {
        slotArray[slot] = cache;
    }
    return cache.passed;
}

} // namespace

// ===========================================================================
// Construction: install the default test list.
// ===========================================================================
//
// Each test below is a small, self-contained correctness check on a single
// math primitive.  Group them however you want; the order here is the
// visible order in the ImGui list.
TestModule_ReviewMath::TestModule_ReviewMath() {
    // The default test list.  Each lambda captures its slot index in
    // gLastCache so the runReviewTest() helper can write the rich per-EXPECT
    // failure log back into a slot that survives past the lambda's return.
    // That log is what shows up under FAIL rows in the ImGui panel and in
    // the console pane -- without this the [FAIL] lines would have no
    // detail.
    tests_ = {
        // ---- Vector family ------------------------------------------------
        { "Vector2  - arithmetic / dot / length / normalize / lerp",
          []{ CachedRun c; return runReviewTest(&review::math::test_Vector2_Basics,    c, gLastCache, gLastCacheSize, 0); } },
        { "Vector3  - arithmetic / dot / cross / length / normalize",
          []{ CachedRun c; return runReviewTest(&review::math::test_Vector3_Basics,    c, gLastCache, gLastCacheSize, 1); } },
        { "Vector4  - arithmetic / dot / perspective divide",
          []{ CachedRun c; return runReviewTest(&review::math::test_Vector4_Basics,    c, gLastCache, gLastCacheSize, 2); } },

        // ---- Matrix4x4 ----------------------------------------------------
        { "Matrix4x4 - identity / multiply / zero",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Identity,  c, gLastCache, gLastCacheSize, 3); } },
        { "Matrix4x4 - transpose",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Transpose, c, gLastCache, gLastCacheSize, 4); } },
        { "Matrix4x4 - inverse (A * A^-1 == I)",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Inverse,   c, gLastCache, gLastCacheSize, 5); } },
        { "Matrix4x4 - translation factory",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Translation, c, gLastCache, gLastCacheSize, 6); } },
        { "Matrix4x4 - scale factory",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Scale,    c, gLastCache, gLastCacheSize, 7); } },
        { "Matrix4x4 - rotation X/Y/Z (orthogonal + axis mapping)",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Rotation, c, gLastCache, gLastCacheSize, 8); } },
        { "Matrix4x4 - perspective projection",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_Perspective, c, gLastCache, gLastCacheSize, 9); } },
        { "Matrix4x4 - lookAt view matrix",
          []{ CachedRun c; return runReviewTest(&review::math::test_Matrix4x4_LookAt,   c, gLastCache, gLastCacheSize, 10); } },

        // ---- Color and utils ----------------------------------------------
        { "Color    - construction / arithmetic / lerp / factories",
          []{ CachedRun c; return runReviewTest(&review::math::test_Color_Basics,   c, gLastCache, gLastCacheSize, 11); } },
        { "MathUtils - deg/rad / clamp / lerp / isZero / isEqual",
          []{ CachedRun c; return runReviewTest(&review::math::test_MathUtils,     c, gLastCache, gLastCacheSize, 12); } },
    };

    // Pre-size the per-test result cache so renderControls() can index
    // into it without reallocation during paint.
    lastResults_.assign(tests_.size(), Result{});
    gLastCacheSize = (int)tests_.size();
    // Clear any cached detail from a previous run.
    for (int i = 0; i < gLastCacheSize; ++i) gLastCache[i] = CachedRun{};
}

// ===========================================================================
// runAll: drives every test in declaration order, updates the roll-up
// state used by the canvas + ImGui panel + console.
// ===========================================================================
void TestModule_ReviewMath::runAll() {
    lastPassed_ = 0;
    lastFailed_ = 0;
    lastTotal_  = (int)tests_.size();
    lastResults_.assign(tests_.size(), Result{});

    for (size_t i = 0; i < tests_.size(); ++i) {
        if (!tests_[i].run) continue;       // user-added entries without a fn
        bool ok = false;
        try { ok = tests_[i].run(); }
        catch (...) { ok = false; }
        lastResults_[i].ran    = true;
        lastResults_[i].passed = ok;
        if (ok) ++lastPassed_; else ++lastFailed_;
    }
    lastState_ = (lastFailed_ == 0 && lastTotal_ > 0)
                     ? RunState::AllPassed
                     : RunState::AnyFailed;
    hasNewResult_ = true;
}

// ===========================================================================
// render: the entire canvas is the verdict colour.
//   - NeverRun      -> dark neutral (18,18,22) so it's obviously "no data"
//   - AllPassed     -> solid green (~40, 130, 60)
//   - AnyFailed     -> solid red   (~150, 40, 40)
//
// Nothing else is drawn: no badges, no separators, no stripes.  The canvas
// is a pure, unambiguous colour signal so any residual pixels from a
// previous render (e.g. leftover black lines) are unmistakable.
// ===========================================================================
void TestModule_ReviewMath::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* r = (SDL_Renderer*)canvasTexture;

    SDL_Color fill{18, 18, 22, 255};
    switch (lastState_) {
    case RunState::AllPassed:
        fill = {40, 130, 60, 255};
        break;
    case RunState::AnyFailed:
        fill = {150, 40, 40, 255};
        break;
    case RunState::NeverRun:
    default:
        fill = {18, 18, 22, 255};
        break;
    }

    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderClear(r);

    if (hasNewResult_) hasNewResult_ = false;
}

// ===========================================================================
// renderControls: the "Run all" button + ordered per-test result list.
//
// Layout: title, button, separator, N rows of (name + PASS/FAIL), final
// summary line.  Rows for tests that haven't been run show "--".
// ===========================================================================
bool TestModule_ReviewMath::renderControls() {
    ImGui::Text("Math review -- Run all to verify the core math stack");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Canvas fills green when every test passes, red if any fails.");
    ImGui::Separator();

    if (ImGui::Button("Run all##math")) {
        runAll();
    }
    ImGui::SameLine();
    if (lastState_ != RunState::NeverRun) {
        ImVec4 col = (lastState_ == RunState::AllPassed)
                         ? ImVec4(0.3f, 0.95f, 0.4f, 1.0f)
                         : ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
        char tag[64];
        std::snprintf(tag, sizeof(tag),
                      lastState_ == RunState::AllPassed
                          ? "Last run: %d / %d passed (green)"
                          : "Last run: %d / %d passed (red)",
                      lastPassed_, lastTotal_);
        ImGui::TextColored(col, "%s", tag);
    } else {
        ImGui::TextDisabled("Last run: not yet.");
    }

    ImGui::Separator();
    ImGui::Text("Per-test results:");
    ImGui::Columns(2, "##math_cols", false);
    ImGui::Text("Test"); ImGui::NextColumn();
    ImGui::Text("Status"); ImGui::NextColumn();
    ImGui::Separator();

    for (size_t i = 0; i < tests_.size(); ++i) {
        ImGui::Text("%s", tests_[i].label);
        ImGui::NextColumn();

        const Result& r = lastResults_[i];
        if (!r.ran) {
            ImGui::TextDisabled("--");
        } else {
            ImVec4 col = r.passed
                             ? ImVec4(0.3f, 0.95f, 0.4f, 1.0f)
                             : ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
            ImGui::TextColored(col, "%s", r.passed ? "PASS" : "FAIL");
        }
        ImGui::NextColumn();

        // For failures, drop out of the column layout to show the
        // expandable detail under the row.  Two columns doesn't allow
        // multi-line content cleanly, so the detail spans the full width
        // as a child block.
        if (r.ran && !r.passed) {
            ImGui::Columns(1);
            ImGui::PushID((int)i);
            if (ImGui::TreeNode("details", "Show failure detail")) {
                if ((int)i < gLastCacheSize && !gLastCache[i].failureLog.empty()) {
                    ImGui::TextWrapped("%s", gLastCache[i].failureLog.c_str());
                } else {
                    ImGui::TextDisabled("(no detail captured)");
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
            ImGui::Columns(2, "##math_cols", false);
        }
    }
    ImGui::Columns(1);

    return true;
}

// ===========================================================================
// runConsole: pretty-prints the last run (or runs if nothing's cached) in
// the same per-test format the old auto-suite used, so the existing
// console pane keeps working.
// ===========================================================================
void TestModule_ReviewMath::runConsole(std::string& output) {
    std::ostringstream log;

    // Run fresh if asked implicitly by re-selecting this module -- same
    // "auto-run" behaviour the old suite had.
    static bool gRunOnce = true;   // run on first open per session
    if (gRunOnce || lastState_ == RunState::NeverRun) {
        runAll();
        gRunOnce = false;
    }

    log << "--- Math review ---\n";
    if (lastTotal_ == 0) {
        log << "  (no tests registered)\n";
    } else {
        for (size_t i = 0; i < tests_.size(); ++i) {
            const Result& r = lastResults_[i];
            const char* status = !r.ran ? "not run"
                               : r.passed ? "PASS" : "FAIL";
            log << "  [" << status << "]  " << tests_[i].label << "\n";
            if (i < (size_t)gLastCacheSize && !r.passed && r.ran) {
                std::string detail = gLastCache[i].failureLog;
                if (!detail.empty()) log << detail;
            }
        }
        log << "\n  Summary: " << lastPassed_ << " passed, "
            << lastFailed_ << " failed  (of " << lastTotal_ << ")\n";
    }

    char header[160];
    const char* tag =
        (lastState_ == RunState::AllPassed) ? "PASS" :
        (lastState_ == RunState::AnyFailed) ? "FAIL" : "N/A";
    std::snprintf(header, sizeof(header),
        "[Review / Math]  suite=%s  (%d / %d passed)\n",
        tag, lastPassed_, lastTotal_);

    output = header + log.str();
}
