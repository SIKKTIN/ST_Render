#pragma once

#include "app/module/IModule.hpp"
#include <SDL2/SDL.h>
#include <string>

// "Math" review sub-module -- runs the core math regression suite
// (Vector2/3/4, Matrix4x4, Color, MathUtils) and prints a PASS/FAIL
// summary to the console pane, and paints a status strip on the canvas.
//
// Sitting under the "Review" category, the user-facing intent is: if you
// break the math layer later, this module turns red.  When the 3D
// pipeline misbehaves, this is the first thing you check.
class TestModule_ReviewMath : public IModule {
public:
    const char* getName() const override { return "Math"; }
    const char* getCategory() const override { return "Review"; }
    bool hasConsoleOutput() const override { return true; }
    bool hasRenderOutput() const override { return true; }
    bool needsRealTimeUpdate() const override { return false; }

    void render(void* canvasTexture, int canvasW, int canvasH) override;
    void runConsole(std::string& output) override;
    bool renderControls() override;

private:
    // Cached latest test result so the canvas strip stays consistent with
    // the most recent console run.  -1 = not run yet.
    mutable int  lastPassed_   = -1;
    mutable int  lastFailed_   = -1;
    mutable int  lastChecks_   = -1;
    mutable bool lastRanOK_    = false;
};
