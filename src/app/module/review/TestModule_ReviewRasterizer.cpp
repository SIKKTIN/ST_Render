#include "TestModule_ReviewRasterizer.hpp"

#include "MathReview_RasterizerHarness.hpp"
#include "MathReview_RasterizerTests.hpp"

#include <cstdio>
#include <cmath>
#include <imgui.h>
#include <sstream>

namespace {

struct CachedRun {
    bool        passed      = false;
    bool        ran         = false;
    std::string failureLog;
};

static CachedRun gLastCache[32];
static int       gLastCacheSize = 0;

static bool runReviewTest(void (*fn)(review::rasterizer::RasterizerCtx&),
                          CachedRun& cache,
                          CachedRun* slotArray,
                          int slotCount,
                          int slot) {
    review::rasterizer::RasterizerCtx ctx;
    ctx.log.str(""); ctx.log.clear();
    fn(ctx);
    cache.ran      = true;
    cache.passed   = (ctx.failed == 0);
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
TestModule_ReviewRasterizer::TestModule_ReviewRasterizer() {
    tests_ = {
        // ---- Barycentric ----------------------------------------------------
        { "Barycentric - corners and center",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Barycentric_CornersAndCenter,   c, gLastCache, gLastCacheSize, 0); } },
        { "Barycentric - edge and outside",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Barycentric_EdgeAndOutside,      c, gLastCache, gLastCacheSize, 1); } },
        { "Barycentric - weight property",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Barycentric_WeightProperty,      c, gLastCache, gLastCacheSize, 2); } },

        // ---- Interpolators -------------------------------------------------
        { "LinearInterpolate - endpoints / color",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_LinearInterpolate,               c, gLastCache, gLastCacheSize, 3); } },
        { "PerspectiveCorrect - uniform W",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_PerspectiveCorrect_UniformW,     c, gLastCache, gLastCacheSize, 4); } },
        { "PerspectiveCorrect - varying W",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_PerspectiveCorrect_VaryingW,      c, gLastCache, gLastCacheSize, 5); } },

        // ---- Rasterization ------------------------------------------------
        { "Rasterize - solid triangle fill",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Rasterize_SolidTriangle,         c, gLastCache, gLastCacheSize, 6); } },
        { "Rasterize - 1-pixel triangle",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Rasterize_TinyOnePixelTriangle,  c, gLastCache, gLastCacheSize, 7); } },
        { "Rasterize - colour interpolation",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Rasterize_ColorInterpolation,     c, gLastCache, gLastCacheSize, 8); } },

        // ---- Depth buffer -------------------------------------------------
        { "DepthBuffer - closer overwrites farther",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_DepthBuffer_OverwriteCloser,     c, gLastCache, gLastCacheSize, 9); } },
        { "DepthBuffer - farther does not overwrite",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_DepthBuffer_FartherDoesNotOverwrite, c, gLastCache, gLastCacheSize, 10); } },
        { "DepthBuffer - equal depth / out-of-bounds",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_DepthBuffer_EqualDepth,           c, gLastCache, gLastCacheSize, 11); } },

        // ---- Viewport / screen-space --------------------------------------
        { "Viewport - raw screen coords",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_ToScreenSpace_RawCoords,         c, gLastCache, gLastCacheSize, 12); } },
        { "Viewport - NDC + viewport transform",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_ToScreenSpace_NDCViewport,      c, gLastCache, gLastCacheSize, 13); } },
        { "Viewport - clamp to buffer bounds",
          []{ CachedRun c; return runReviewTest(&review::rasterizer::test_Viewport_Clamp,                  c, gLastCache, gLastCacheSize, 14); } },
    };

    lastResults_.assign(tests_.size(), Result{});
    gLastCacheSize = (int)tests_.size();
    for (int i = 0; i < gLastCacheSize; ++i) gLastCache[i] = CachedRun{};
}

// ===========================================================================
// runAll
// ===========================================================================
void TestModule_ReviewRasterizer::runAll() {
    lastPassed_ = 0;
    lastFailed_ = 0;
    lastTotal_  = (int)tests_.size();
    lastResults_.assign(tests_.size(), Result{});

    for (size_t i = 0; i < tests_.size(); ++i) {
        if (!tests_[i].run) continue;
        bool ok = false;
        try { ok = tests_[i].run(); }
        catch (...) { ok = false; }
        lastResults_[i].ran    = true;
        lastResults_[i].passed = ok;
        if (ok) ++lastPassed_; else ++lastFailed_;
    }
    lastState_    = (lastFailed_ == 0 && lastTotal_ > 0)
                        ? RunState::AllPassed
                        : RunState::AnyFailed;
    hasNewResult_ = true;
}

// ===========================================================================
// render: pure colour verdict canvas.
// ===========================================================================
void TestModule_ReviewRasterizer::render(void* canvasTexture, int canvasW, int canvasH) {
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
// renderControls
// ===========================================================================
bool TestModule_ReviewRasterizer::renderControls() {
    ImGui::Text("Rasterizer review -- Run all to verify triangle rasterization");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
        "Canvas fills green when every test passes, red if any fails.");
    ImGui::Separator();

    if (ImGui::Button("Run all##rasterizer")) {
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
    ImGui::Columns(2, "##rast_cols", false);
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
            ImGui::Columns(2, "##rast_cols", false);
        }
    }
    ImGui::Columns(1);

    return true;
}

// ===========================================================================
// runConsole
// ===========================================================================
void TestModule_ReviewRasterizer::runConsole(std::string& output) {
    std::ostringstream log;

    static bool gRunOnce = true;
    if (gRunOnce || lastState_ == RunState::NeverRun) {
        runAll();
        gRunOnce = false;
    }

    log << "--- Rasterizer review ---\n";
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
        "[Review / Rasterizer]  suite=%s  (%d / %d passed)\n",
        tag, lastPassed_, lastTotal_);

    output = header + log.str();
}
