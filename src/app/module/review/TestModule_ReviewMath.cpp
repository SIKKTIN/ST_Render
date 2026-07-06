#include "TestModule_ReviewMath.hpp"

#include "MathReview_Harness.hpp"
#include "MathReview_VectorTests.hpp"
#include "MathReview_MatrixTests.hpp"
#include "MathReview_ColorAndUtilsTests.hpp"

#include <cstdio>
#include <cmath>
#include <imgui.h>

namespace {

// Runnable test table. Each entry is a named, side-effecting function that
// exercises a single piece of the math layer.  Order matters for the
// summary -- basic ops first, then factories, then transforms.
struct MathTest {
    const char* label;
    void (*fn)(review::math::ReviewCtx&);
};

static const MathTest kMathTests[] = {
    { "Vector2  - arithmetic / dot / length / normalize / lerp",  &review::math::test_Vector2_Basics },
    { "Vector3  - arithmetic / dot / cross / length / normalize",  &review::math::test_Vector3_Basics },
    { "Vector4  - arithmetic / dot / perspective divide",          &review::math::test_Vector4_Basics },
    { "Matrix4x4 - identity / multiply / zero",                   &review::math::test_Matrix4x4_Identity },
    { "Matrix4x4 - transpose",                                    &review::math::test_Matrix4x4_Transpose },
    { "Matrix4x4 - inverse (A * A^-1 == I)",                      &review::math::test_Matrix4x4_Inverse },
    { "Matrix4x4 - translation factory",                          &review::math::test_Matrix4x4_Translation },
    { "Matrix4x4 - scale factory",                                &review::math::test_Matrix4x4_Scale },
    { "Matrix4x4 - rotation X/Y/Z (orthogonal + axis mapping)",    &review::math::test_Matrix4x4_Rotation },
    { "Matrix4x4 - perspective projection",                       &review::math::test_Matrix4x4_Perspective },
    { "Matrix4x4 - lookAt view matrix",                           &review::math::test_Matrix4x4_LookAt },
    { "Color    - construction / arithmetic / lerp / factories",   &review::math::test_Color_Basics },
    { "MathUtils - deg/rad / clamp / lerp / isZero / isEqual",    &review::math::test_MathUtils },
};

static int  gSelectedTest = 0;       // for the controls-panel jump menu
static bool gAutoRun      = true;    // rerun every time the module is shown

void runAllMathTests(int& passed, int& failed, int& checks, std::ostringstream& log) {
    passed = 0; failed = 0; checks = 0;
    log.str(""); log.clear();

    for (const auto& t : kMathTests) {
        review::math::ReviewCtx sub;
        log << "  " << t.label << "\n";
        t.fn(sub);
        passed += sub.passed;
        failed += sub.failed;
        checks += sub.checks;
        log << sub.log.str();
        log << "    -> " << (sub.failed == 0 ? "PASS" : "FAIL")
            << "  (" << sub.checks << " checks)\n\n";
    }
}

} // namespace

void TestModule_ReviewMath::render(void* canvasTexture, int canvasW, int canvasH) {
    SDL_Renderer* r = (SDL_Renderer*)canvasTexture;

    // Background -- neutral gray.  A vertical strip on the left reflects the
    // latest verdict: green = all green, red = something failed, dark gray
    // = not run yet.
    SDL_SetRenderDrawColor(r, 18, 18, 22, 255);
    SDL_RenderClear(r);

    if (lastPassed_ < 0) {
        SDL_SetRenderDrawColor(r, 60, 60, 65, 255);
    } else if (lastFailed_ == 0) {
        SDL_SetRenderDrawColor(r, 40, 110, 60, 255);
    } else {
        SDL_SetRenderDrawColor(r, 130, 40, 40, 255);
    }
    SDL_Rect strip = { 0, 0, 6, canvasH };
    SDL_RenderFillRect(r, &strip);

    // Top-right verdict badge.
    SDL_SetRenderDrawColor(r, lastFailed_ == 0 ? 40 : 130,
                               lastFailed_ == 0 ? 110 : 40,
                               lastFailed_ == 0 ? 60 : 40, 255);
    SDL_Rect badge = { canvasW - 200, 0, 200, 36 };
    SDL_RenderFillRect(r, &badge);

    // Thin separator under the badge.
    SDL_SetRenderDrawColor(r, 60, 60, 65, 255);
    SDL_RenderDrawLine(r, 0, 36, canvasW, 36);

    if (lastRanOK_) needsRerender = false;
}

bool TestModule_ReviewMath::renderControls() {
    ImGui::Text("Math review -- verify the core types (Vector2/3/4, Matrix4x4, Color, MathUtils).");
    ImGui::Separator();

    if (ImGui::Button("Run all")) {
        std::ostringstream tmp;
        runAllMathTests(lastPassed_, lastFailed_, lastChecks_, tmp);
        // Also push the result into the console pane on demand.
        // (The console pane is also refreshed when the module is selected
        // by the main loop; see runConsole().)
        lastRanOK_ = true;
        needsRerender = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-run on select", &gAutoRun);

    if (lastPassed_ >= 0) {
        ImGui::Separator();
        if (lastFailed_ == 0) {
            ImGui::TextColored(ImVec4(0.3f, 0.95f, 0.4f, 1.0f),
                "ALL PASS  (%d tests, %d checks)", lastPassed_, lastChecks_);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                "FAILED    (%d / %d tests failed, %d checks)",
                lastFailed_, lastPassed_ + lastFailed_, lastChecks_);
        }
        ImGui::TextDisabled("Open the console pane below for per-test details.");
    } else {
        ImGui::TextDisabled("Press 'Run all' or select this module again to auto-run.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Per-test list (read-only -- runs in order):");
    for (size_t i = 0; i < sizeof(kMathTests) / sizeof(kMathTests[0]); ++i) {
        ImGui::BulletText("%s", kMathTests[i].label);
    }

    return true;
}

void TestModule_ReviewMath::runConsole(std::string& output) {
    std::ostringstream log;
    if (gAutoRun || lastPassed_ < 0) {
        runAllMathTests(lastPassed_, lastFailed_, lastChecks_, log);
        lastRanOK_ = true;
        needsRerender = true;
    } else {
        // Re-emit the last summary so the user can scroll back without
        // triggering a fresh rerun.
        log << "  (showing results from previous run -- press 'Run all' "
               "or toggle Auto-run to refresh)\n\n";
    }

    char summary[128];
    std::snprintf(summary, sizeof(summary),
        "[Review / Math]  %d / %d tests passed, %d checks  -> %s\n",
        lastPassed_,
        lastPassed_ + lastFailed_,
        lastChecks_,
        lastFailed_ == 0 ? "PASS" : "FAIL");

    output = summary + log.str();
}
