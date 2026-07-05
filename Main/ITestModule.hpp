#pragma once

#include <string>

class ITestModule {
public:
    virtual ~ITestModule() = default;

    virtual const char* getName() const = 0;
    virtual bool hasRenderOutput() const { return true; }
    virtual bool hasConsoleOutput() const { return true; }
    virtual bool needsRealTimeUpdate() const { return false; }

    virtual void update(float deltaTime) {}
    virtual void render(void* canvasTexture, int canvasW, int canvasH) {}
    virtual bool renderControls() { return false; }
    virtual void renderOverlays() {}
    virtual void renderUIOverlay(int canvasX, int canvasY, int canvasW, int canvasH) {}
    virtual void renderCreatePanel() {}
    virtual void setRenderer(void* renderer) {}
    virtual void runConsole(std::string& output) {}

    virtual void onMouseMove(int x, int y) {}
    virtual void onMouseDown(int button, int x, int y) {}
    virtual void onMouseUp(int button) {}
    virtual void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) {}

    // Canvas-space mouse events (for UI element interaction)
    virtual void onCanvasMouseMove(int canvasX, int canvasY) {}
    virtual void onCanvasMouseDown(int button, int canvasX, int canvasY) {}
    virtual void onCanvasMouseUp(int button, int canvasX, int canvasY) {}

    // Keyboard events
    virtual void onKeyDown(int keycode) {}

    bool needsRerender = false;
};
