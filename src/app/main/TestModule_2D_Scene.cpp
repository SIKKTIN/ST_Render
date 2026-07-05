#include "TestModule_2D_Scene.hpp"
#include "core/data/DataBase/SceneData.hpp"
#include "engine/editor/Cinemachine.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <shobjidl.h>
#include <objbase.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <iostream>
#include <functional>

ST::Matrix4x4 buildModelMatrix(const ST::GameObject* obj);
ST::Matrix4x4 buildWorldMatrix(const ST::GameObject* obj);
ST::Matrix4x4 buildModelMatrix3D(const ST::GameObject* obj);
ST::Matrix4x4 buildWorldMatrix3D(const ST::GameObject* obj);
void appendSceneHierarchyEntries(ST::Scene& scene, const std::vector<ST::GameObject*>& objects, ST::GameObject*& selected, bool& needsRerender);
void renderHierarchyPanel(ST::Scene& scene, ST::GameObject*& selected, bool& needsRerender);

float wrapFraction(float x) {
    x = std::fmod(x, 1.0f);
    if (x < 0) x += 1.0f;
    return x;
}

ST::Matrix4x4 buildModelMatrix3D(const ST::GameObject* obj) {
    // Full TRS for 3D (X/Y/Z rotations supported).
    // Mirrors transform/Transform.hpp::getModelMatrix but is defined here so the
    // 2D buildModelMatrix stays untouched and continues to use Z-only rotation.
    ST::Matrix4x4 T = ST::Matrix4x4::translation(obj->position.x, obj->position.y, obj->position.z);
    ST::Matrix4x4 Rx = ST::Matrix4x4::rotationX(obj->rotation.x);
    ST::Matrix4x4 Ry = ST::Matrix4x4::rotationY(obj->rotation.y);
    ST::Matrix4x4 Rz = ST::Matrix4x4::rotationZ(obj->rotation.z);
    ST::Matrix4x4 S  = ST::Matrix4x4::scale(obj->scale.x, obj->scale.y, obj->scale.z);
    return T * Ry * Rx * Rz * S;
}

ST::Matrix4x4 buildWorldMatrix3D(const ST::GameObject* obj) {
    ST::Matrix4x4 local = buildModelMatrix3D(obj);
    if (obj && obj->parent) {
        return buildWorldMatrix3D(obj->parent) * local;
    }
    return local;
}


TestModule_2D_Scene::TestModule_2D_Scene()
    : m_frameBuffer(nullptr)
    , m_ssaaBuffer(nullptr)
    , m_depthBuffer(nullptr)
    , m_ssaaDepthBuffer(nullptr)
    , m_canvasW(640)
    , m_canvasH(480)
    , m_canvasTop(50)
    , m_zoom(1.0f)
    , m_panX(0.0f)
    , m_panY(0.0f)
    , m_gridSize(100.0f)
    , m_dragging(false)
    , m_lastMouseX(0), m_lastMouseY(0)
    , m_showGrid(true)
    , m_aaMode(0)
    , m_viewportMode(0)
    , m_gameRunning(false)
    , m_gameResolutionIndex(9)
    , m_ssaaLevel(2)
    , m_selected(nullptr)
    , m_objDragging(false)
    , m_objDragStartX(0), m_objDragStartY(0)
    , m_objDragStartObjX(0.0f), m_objDragStartObjY(0.0f)
    , m_objDragStartWX(0.0f), m_objDragStartWY(0.0f)
{
    CoInitialize(nullptr);
    m_scene.setTransformUI(&m_transformUI);

    // 2D defaults (preserved from prior versions)
    auto* yellow = m_scene.createGameObject("YellowQuad");
    yellow->setPosition(0.0f, 0.0f, 0.0f);
    yellow->setColor(1.0f, 1.0f, 0.0f, 1.0f);
    yellow->createQuad(200.0f, 200.0f);

    auto* red = m_scene.createGameObject("RedQuad");
    red->setPosition(300.0f, 0.0f, 0.0f);
    red->setColor(1.0f, 0.0f, 0.0f, 1.0f);
    red->createQuad(200.0f, 200.0f);

    // 3D defaults — a small group of cubes around origin so 3D view has content
    auto* cubeA = m_scene.createGameObject("Cube_Red");
    cubeA->setPosition(-150.0f, 0.0f, 0.0f);
    cubeA->setColor(0.9f, 0.3f, 0.3f, 1.0f);
    cubeA->createCube(120.0f, 120.0f, 120.0f);

    auto* cubeB = m_scene.createGameObject("Cube_Green");
    cubeB->setPosition(150.0f, 0.0f, 0.0f);
    cubeB->setColor(0.3f, 0.9f, 0.3f, 1.0f);
    cubeB->createCube(120.0f, 120.0f, 120.0f);

    auto* cubeC = m_scene.createGameObject("Cube_Blue");
    cubeC->setPosition(0.0f, 150.0f, 0.0f);
    cubeC->setColor(0.3f, 0.4f, 0.9f, 1.0f);
    cubeC->createCube(120.0f, 120.0f, 120.0f);

    auto* ground = m_scene.createGameObject("Ground");
    ground->setPosition(0.0f, 0.0f, 200.0f);
    ground->setScale(800.0f, 800.0f, 1.0f);
    ground->setColor(0.3f, 0.3f, 0.35f, 1.0f);
    ground->createCube(1.0f, 1.0f, 1.0f); // unit cube scaled to plane by setScale
    ground->sortingOrder = -1; // render first

    // Configure scene camera with sensible 3D defaults.
    m_scene.camera.m_eye    = ST::Vector3(400.0f, 300.0f, 700.0f);
    m_scene.camera.m_target = ST::Vector3(0.0f, 0.0f, 0.0f);
    m_scene.camera.m_up     = ST::Vector3(0.0f, 1.0f, 0.0f);
    m_scene.camera.m_mode   = ST::ProjectionMode::Perspective;
    m_scene.camera.m_fov    = 3.14159f / 4.0f;
    m_scene.camera.m_aspect = 16.0f / 9.0f;
    m_scene.camera.m_near   = 0.1f;
    m_scene.camera.m_far    = 10000.0f;

    // Initialise orbit controller to match (approximate spherical from eye).
    m_orbit.setTarget(ST::Vector3(0, 0, 0));
    // Pick a yaw that points the camera roughly toward +Z from the default eye.
    m_orbit.setYaw(-0.6f);
    m_orbit.setPitch(0.35f);
    m_orbit.setRadius(900.0f);
    m_orbit.applyTo(m_scene.camera);
}

void TestModule_2D_Scene::resetOrbitCamera() {
    m_orbit.resetToDefault();
    m_orbit.applyTo(m_scene.camera);
}

void TestModule_2D_Scene::setView3D(bool v) {
    if (m_view3D == v) return;
    m_view3D = v;
    if (m_view3D) {
        m_orbit.applyTo(m_scene.camera);
    }
    needsRerender = true;
}

TestModule_2D_Scene::~TestModule_2D_Scene() {
    m_scene.stopRuntime();
    delete m_frameBuffer;
    delete m_ssaaBuffer;
    delete m_depthBuffer;
    delete m_ssaaDepthBuffer;
}

void TestModule_2D_Scene::updateViewFromCamera() {
    m_scene.viewportZoom = m_zoom;
    m_scene.viewportOffset.x = m_panX;
    m_scene.viewportOffset.y = m_panY;
}

void TestModule_2D_Scene::syncGameRuntimeState() {
    if (m_viewportMode == 1 && m_gameRunning) {
        m_scene.startRuntime();
    } else {
        m_scene.stopRuntime();
    }
}

void TestModule_2D_Scene::updateCinemachine(float deltaTime) {
    if (m_viewportMode == 0) return;

    if (m_gameRunning) {
        m_scene.updateRuntime(deltaTime);
    }

    ST::Cinemachine* mainCam = m_scene.findCinemachine();

    if (mainCam) {
        m_scene.viewportOffset.x = mainCam->position.x;
        m_scene.viewportOffset.y = mainCam->position.y;
        m_scene.viewportZoom = 1.0f;
    } else {
        m_scene.viewportOffset.x = 0.0f;
        m_scene.viewportOffset.y = 0.0f;
        m_scene.viewportZoom = 1.0f;
    }
}

bool TestModule_2D_Scene::isInCanvasScreen(int sx, int sy, int canvasW, int canvasH) const {
    const float leftPanelW = 220.0f;
    const float createObjPanelW = 200.0f;
    int screenCanvasX0 = (int)leftPanelW + (int)createObjPanelW;
    return sx >= screenCanvasX0 && sx < screenCanvasX0 + canvasW
        && sy >= 0 && sy < canvasH;
}

bool TestModule_2D_Scene::isInCanvas(int x, int y, int canvasW, int canvasH) const {
    return x >= 0 && x < canvasW && y >= 0 && y < canvasH;
}

int TestModule_2D_Scene::toCanvasX(int screenX) const {
    const float leftPanelW = 220.0f;
    const float createObjPanelW = 200.0f;
    return screenX - (int)(leftPanelW + createObjPanelW);
}

void TestModule_2D_Scene::onMouseDown(int button, int x, int y) {
    if (!isInCanvasScreen(x, y, m_canvasW, m_canvasH)) return;

    int cx = toCanvasX(x);
    int cy = y - m_canvasTop;

    if (m_viewportMode == 1) {
        if (button == 1) {
            m_objDragging = false;
        }
        return;
    }

    // 3D view: right-mouse orbits, middle-mouse pans target (later), left-mouse picks.
    if (m_view3D) {
        if (button == 3) { // RMB
            m_orbitDragging = true;
            m_orbitLastX = x;
            m_orbitLastY = y;
        } else if (button == 1) { // LMB
            ST::GameObject* hit = pickObject3D(cx, cy, m_canvasW, m_canvasH);
            if (hit) {
                m_selected = hit;
                needsRerender = true;
            }
        }
        return;
    }

    if (button == 3) {
        m_dragging = true;
        m_lastMouseX = x;
        m_lastMouseY = y;
        return;
    }

    if (button == 1) {
        int cx = toCanvasX(x);
        int cy = y - m_canvasTop;

        ST::GameObject* topObj = nullptr;
        int topSortingOrder = INT_MIN;

        ST::Matrix4x4 view = ST::Matrix4x4::translation(m_panX, m_panY, 0.0f) *
                             ST::Matrix4x4::scale(m_zoom, m_zoom, 1.0f);
        float halfW = m_canvasW * 0.5f / m_zoom;
        float halfH = m_canvasH * 0.5f / m_zoom;
        ST::Matrix4x4 projection = ST::Matrix4x4::orthographic(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);

        std::vector<ST::GameObject*> allObjs;
        m_scene.getSortedObjects(allObjs);

        for (ST::GameObject* obj : allObjs) {
            if (obj->sortingOrder < topSortingOrder) continue;

            ST::Matrix4x4 model = buildWorldMatrix(obj);
            ST::Matrix4x4 mvp = projection * view * model;

            const auto& verts = obj->getMesh().getVertices();
            float minSX = FLT_MAX, maxSX = -FLT_MAX;
            float minSY = FLT_MAX, maxSY = -FLT_MAX;

            for (const auto& v : verts) {
                ST::Vector4 clip = mvp * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
                float invW = 1.0f / clip.w;
                float sx = (clip.x * invW + 1.0f) * 0.5f * m_canvasW;
                float sy = (1.0f - clip.y * invW) * 0.5f * m_canvasH;
                if (sx < minSX) minSX = sx;
                if (sx > maxSX) maxSX = sx;
                if (sy < minSY) minSY = sy;
                if (sy > maxSY) maxSY = sy;
            }

            if (cx >= minSX && cx <= maxSX && cy >= minSY && cy <= maxSY) {
                topObj = obj;
                topSortingOrder = obj->sortingOrder;
            }
        }

        if (topObj) {
            m_selected = topObj;
            m_objDragging = true;
            m_objDragStartX = x;
            m_objDragStartY = y;
            m_objDragStartObjX = m_selected->position.x;
            m_objDragStartObjY = m_selected->position.y;
            float wx, wy;
            screenToWorld(cx, cy, m_canvasW, m_canvasH, wx, wy);
            m_objDragStartWX = wx;
            m_objDragStartWY = wy;
        }
        // NOTE: do NOT clear m_selected when clicking empty canvas here.
        // Clearing it would interfere with ImGui widget clicks (e.g. Spawn button)
        // in the right panel, because the canvas detection bounds may overlap.
        // Selection is cleared only when clicking empty space in the actual canvas.
    }
}

void TestModule_2D_Scene::onMouseUp(int button) {
    if (button == 3) {
        m_dragging = false;
        m_orbitDragging = false;
    }
    if (button == 1) {
        m_objDragging = false;
    }
}

void TestModule_2D_Scene::onCanvasMouseMove(int cx, int cy) {
    if (m_dragging) {
        int dx = cx - m_lastMouseX;
        int dy = cy - m_lastMouseY;
        m_panX += dx / m_zoom;
        m_panY -= dy / m_zoom;
        m_lastMouseX = cx;
        m_lastMouseY = cy;
        updateViewFromCamera();
    }

    if (m_objDragging && m_selected) {
        float wx, wy;
        screenToWorld(cx, cy, m_canvasW, m_canvasH, wx, wy);
        float dx = wx - m_objDragStartWX;
        float dy = wy - m_objDragStartWY;
        m_selected->position.x = m_objDragStartObjX + dx;
        m_selected->position.y = m_objDragStartObjY + dy;
    }
}

void TestModule_2D_Scene::onCanvasMouseDown(int button, int cx, int cy) {
    if (m_viewportMode == 1) return;

    if (m_view3D) {
        // 3D: RMB for orbit, LMB for picking.
        if (button == 3) {
            m_orbitDragging = true;
            m_orbitLastX = cx;
            m_orbitLastY = cy;
        } else if (button == 1) {
            ST::GameObject* hit = pickObject3D(cx, cy, m_canvasW, m_canvasH);
            if (hit) {
                m_selected = hit;
                needsRerender = true;
            }
        }
        return;
    }

    if (button == 3) {
        m_dragging = true;
        m_lastMouseX = cx;
        m_lastMouseY = cy;
    }
}

void TestModule_2D_Scene::onCanvasMouseUp(int button, int cx, int cy) {
    if (button == 3) {
        m_dragging = false;
        m_orbitDragging = false;
    }
    if (button == 1) {
        m_objDragging = false;
    }
    (void)cx;
    (void)cy;
}

void TestModule_2D_Scene::onKeyDown(int keycode) {
    m_keyHeld[keycode] = true;

    if (keycode == SDLK_f) {
        if (m_selected) {
            if (m_view3D) {
                // 3D: move orbit target onto selected object's position.
                m_orbit.setTarget(m_selected->position);
                m_orbit.applyTo(m_scene.camera);
            } else {
                m_panX = m_selected->position.x;
                m_panY = m_selected->position.y;
                m_scene.viewportOffset.x = m_panX;
                m_scene.viewportOffset.y = m_panY;
            }
            std::cout << "[Scene] Focused on: " << m_selected->name << std::endl;
        }
    } else if (keycode == SDLK_v) {
        // 'V' toggles 2D / 3D scene view (scene mode only).
        if (m_viewportMode == 0) setView3D(!m_view3D);
    } else if (keycode == SDLK_r && m_view3D) {
        // 'R' resets orbit camera.
        resetOrbitCamera();
    }
}

void TestModule_2D_Scene::onKeyUp(int keycode) {
    m_keyHeld[keycode] = false;
}

void TestModule_2D_Scene::update(float deltaTime) {
    if (!m_view3D || m_viewportMode != 0) return;

    // WASD/QE pan for 3D orbit camera.
    const bool w = m_keyHeld[SDLK_w];
    const bool a = m_keyHeld[SDLK_a];
    const bool s = m_keyHeld[SDLK_s];
    const bool d = m_keyHeld[SDLK_d];
    const bool q = m_keyHeld[SDLK_q];
    const bool e = m_keyHeld[SDLK_e];
    if (w || a || s || d || q || e) {
        // Speed scales with distance so far targets pan slower (more natural).
        float panAmount = (w ? -1.0f : 0.0f) + (s ? 1.0f : 0.0f);
        float rightAmount = (d ? 1.0f : 0.0f) + (a ? -1.0f : 0.0f);
        float upAmount    = (e ? 1.0f : 0.0f) + (q ? -1.0f : 0.0f);
        float dt = deltaTime * 60.0f; // scale so default feels right at 60fps
        m_orbit.panLocal(panAmount * dt, rightAmount * dt, upAmount * dt);
        m_orbit.applyTo(m_scene.camera);
        needsRerender = true;
    }
}

void TestModule_2D_Scene::onMouseMove(int x, int y) {
    if (m_orbitDragging && m_view3D && m_viewportMode == 0) {
        int dx = x - m_orbitLastX;
        int dy = y - m_orbitLastY;
        m_orbit.onOrbit(dx, dy);
        m_orbit.applyTo(m_scene.camera);
        m_orbitLastX = x;
        m_orbitLastY = y;
        needsRerender = true;
    }

    if (m_dragging) {
        int dx = x - m_lastMouseX;
        int dy = y - m_lastMouseY;
        m_panX += dx / m_zoom;
        m_panY -= dy / m_zoom;
        m_lastMouseX = x;
        m_lastMouseY = y;
        updateViewFromCamera();
    }

    if (m_objDragging && m_selected) {
        int cx = toCanvasX(x);
        int cy = y - m_canvasTop;
        float wx, wy;
        screenToWorld(cx, cy, m_canvasW, m_canvasH, wx, wy);
        float dx = wx - m_objDragStartWX;
        float dy = wy - m_objDragStartWY;
        m_selected->position.x = m_objDragStartObjX + dx;
        m_selected->position.y = m_objDragStartObjY + dy;
    }
}

void TestModule_2D_Scene::onWheel(float dx, float dy, int cx, int cy, int canvasW, int canvasH) {
    if (!isInCanvas(cx, cy, canvasW, canvasH)) return;

    if (m_view3D && m_viewportMode == 0) {
        m_orbit.onZoom(dy);
        m_orbit.applyTo(m_scene.camera);
        needsRerender = true;
        (void)dx;
        return;
    }

    float oldZoom = m_zoom;
    if (dy > 0) {
        m_zoom *= 1.1f;
    } else {
        m_zoom *= 0.9f;
    }
    m_zoom = std::clamp(m_zoom, 0.05f, 50.0f);

    float centerX = canvasW * 0.5f;
    float centerY = canvasH * 0.5f;

    float worldX = (cx - centerX) / oldZoom - m_panX;
    float worldY = (centerY - cy) / oldZoom - m_panY;

    m_panX = (cx - centerX) / m_zoom - worldX;
    m_panY = (centerY - cy) / m_zoom - worldY;
    updateViewFromCamera();
}

void TestModule_2D_Scene::screenToWorld(int sx, int sy, int canvasW, int canvasH, float& wx, float& wy) {
    float centerX = canvasW * 0.5f;
    float centerY = canvasH * 0.5f;
    wx = (sx - centerX) / m_zoom - m_panX;
    wy = (centerY - sy) / m_zoom - m_panY;
}

void TestModule_2D_Scene::rebuildBuffers(int canvasW, int canvasH) {
    if (!m_frameBuffer || m_frameBuffer->getWidth() != canvasW || m_frameBuffer->getHeight() != canvasH) {
        delete m_frameBuffer;
        delete m_depthBuffer;
        m_frameBuffer = new ST::FrameBuffer();
        m_frameBuffer->initialize(canvasW, canvasH);
        m_depthBuffer = new ST::DepthBuffer();
        m_depthBuffer->initialize(canvasW, canvasH);
    }
}

void TestModule_2D_Scene::rebuildSSAABuffer(int canvasW, int canvasH) {
    int ssaaW = canvasW * m_ssaaLevel;
    int ssaaH = canvasH * m_ssaaLevel;
    if (!m_ssaaBuffer || m_ssaaBuffer->getWidth() != ssaaW || m_ssaaBuffer->getHeight() != ssaaH) {
        delete m_ssaaBuffer;
        delete m_ssaaDepthBuffer;
        m_ssaaBuffer = new ST::FrameBuffer();
        m_ssaaBuffer->initialize(ssaaW, ssaaH);
        m_ssaaDepthBuffer = new ST::DepthBuffer();
        m_ssaaDepthBuffer->initialize(ssaaW, ssaaH);
    }
}

void TestModule_2D_Scene::downsample(ST::FrameBuffer* src, ST::FrameBuffer* dst) {
    int dstW = dst->getWidth();
    int dstH = dst->getHeight();
    int level = m_ssaaLevel;

    for (int dy = 0; dy < dstH; dy++) {
        for (int dx = 0; dx < dstW; dx++) {
            float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
            int count = 0;
            for (int sy = 0; sy < level; sy++) {
                for (int sx = 0; sx < level; sx++) {
                    int sx_ = dx * level + sx;
                    int sy_ = dy * level + sy;
                    ST::Color c = src->getPixel(sx_, sy_);
                    r += c.r; g += c.g; b += c.b; a += c.a;
                    count++;
                }
            }
            dst->setPixel(dx, dy, ST::Color(r / count, g / count, b / count, a / count));
        }
    }
}

float fxaaLuminance(const ST::Color& c) {
    return 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
}

void TestModule_2D_Scene::applyFXAA(ST::FrameBuffer* fb) {
    int w = fb->getWidth();
    int h = fb->getHeight();
    int total = w * h;

    const float contrastThreshold = 0.02f;
    const float blendStrength = 0.9f;

    std::vector<ST::Color> result(total);

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;

            float lC = fxaaLuminance(fb->getPixel(x, y));
            float lN = fxaaLuminance(fb->getPixel(x, y - 1));
            float lS = fxaaLuminance(fb->getPixel(x, y + 1));
            float lW = fxaaLuminance(fb->getPixel(x - 1, y));
            float lE = fxaaLuminance(fb->getPixel(x + 1, y));

            float contrast = std::max(lN, std::max(lS, std::max(lW, std::max(lE, lC))))
                           - std::min(lN, std::min(lS, std::min(lW, std::min(lE, lC))));

            if (contrast <= contrastThreshold) {
                result[idx] = fb->getPixel(x, y);
                continue;
            }

            float horz = std::abs(lW - lC) + std::abs(lE - lC);
            float vert = std::abs(lN - lC) + std::abs(lS - lC);
            bool horizontal = (horz >= vert);

            float l1 = horizontal ? lW : lN;
            float l2 = horizontal ? lE : lS;
            int stepOff = horizontal ? -1 : -w;
            int stepOn = horizontal ? 1 : w;

            float gradiant1 = std::abs(lC - l1);
            float gradiant2 = std::abs(lC - l2);
            int stepNeg = (gradiant1 >= gradiant2) ? stepOff : stepOn;

            int neg = idx;
            int pos = idx;
            int lenNeg = 0, lenPos = 0;
            float diffThreshold = contrast * 0.5f;

            for (int i = 0; i < 8; i++) {
                neg += stepNeg;
                if (neg < 0 || neg >= total) break;
                float diff = std::abs(fxaaLuminance(fb->getPixel(neg % w, neg / w)) - lC);
                if (diff > diffThreshold) break;
                lenNeg++;
            }

            int stepPos = -stepNeg;
            for (int i = 0; i < 8; i++) {
                pos += stepPos;
                if (pos < 0 || pos >= total) break;
                float diff = std::abs(fxaaLuminance(fb->getPixel(pos % w, pos / w)) - lC);
                if (diff > diffThreshold) break;
                lenPos++;
            }

            float totalLen = (float)(lenNeg + lenPos);
            if (totalLen < 0.5f) {
                result[idx] = fb->getPixel(x, y);
                continue;
            }

            float edgePos = (float)lenNeg / totalLen;
            float edgeNeg = (float)lenPos / totalLen;

            float blend = (edgeNeg < edgePos ? edgeNeg : edgePos);
            blend = std::max(blend, contrastThreshold);
            blend = std::min(blend * 2.5f, blendStrength);

            int sampleIdx = idx + (int)((float)(lenNeg - lenPos) * 0.5f * edgePos);
            if (sampleIdx < 0) sampleIdx = 0;
            if (sampleIdx >= total) sampleIdx = total - 1;

            ST::Color base = fb->getPixel(x, y);
            ST::Color blendCol = fb->getPixel(sampleIdx % w, sampleIdx / w);

            result[idx].r = base.r * (1.0f - blend) + blendCol.r * blend;
            result[idx].g = base.g * (1.0f - blend) + blendCol.g * blend;
            result[idx].b = base.b * (1.0f - blend) + blendCol.b * blend;
            result[idx].a = base.a;
        }
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int idx = y * w + x;
            if (y == 0 || y == h - 1 || x == 0 || x == w - 1) {
                result[idx] = fb->getPixel(x, y);
            }
            fb->setPixel(x, y, result[idx]);
        }
    }
}

ST::Matrix4x4 buildModelMatrix(const ST::GameObject* obj) {
    // 2D-only: Z rotation. Preserved for the 2D rendering path.
    ST::Matrix4x4 T = ST::Matrix4x4::translation(obj->position.x, obj->position.y, obj->position.z);
    ST::Matrix4x4 R = ST::Matrix4x4::rotationZ(obj->rotation.z);
    ST::Matrix4x4 S = ST::Matrix4x4::scale(obj->scale.x, obj->scale.y, obj->scale.z);
    return T * R * S;
}

ST::Matrix4x4 buildWorldMatrix(const ST::GameObject* obj) {
    ST::Matrix4x4 local = buildModelMatrix(obj);
    if (obj && obj->parent) {
        return buildWorldMatrix(obj->parent) * local;
    }
    return local;
}

void appendSceneHierarchyEntries(ST::Scene& scene, const std::vector<ST::GameObject*>& objects, ST::GameObject*& selected, bool& needsRerender) {
    for (size_t i = 0; i < objects.size(); ++i) {
        ST::GameObject* obj = objects[i];
        bool hasChildren = !obj->children.empty();

        ImGuiTreeNodeFlags flags = 0;
        if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;
        if (selected == obj) flags |= ImGuiTreeNodeFlags_Selected;

        bool opened = ImGui::TreeNodeEx(obj, flags, "%s", obj->name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            selected = obj;
            needsRerender = true;
        }
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(2)) {
            ImGui::OpenPopup(("obj_popup_" + std::to_string(reinterpret_cast<uintptr_t>(obj))).c_str());
        }
        if (ImGui::BeginPopup(("obj_popup_" + std::to_string(reinterpret_cast<uintptr_t>(obj))).c_str())) {
            selected = obj;
            if (ImGui::MenuItem("Delete")) {
                scene.destroyGameObject(obj);
                selected = nullptr;
                needsRerender = true;
                ImGui::EndPopup();
                if (opened) ImGui::TreePop();
                return;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            std::vector<ST::GameObject*> sortedChildren = obj->children;
            std::stable_sort(sortedChildren.begin(), sortedChildren.end(),
                [](const ST::GameObject* a, const ST::GameObject* b) {
                    return a->sortingOrder < b->sortingOrder;
                });
            appendSceneHierarchyEntries(scene, sortedChildren, selected, needsRerender);
            ImGui::TreePop();
        }
    }
}

void renderHierarchyPanel(ST::Scene& scene, ST::GameObject*& selected, bool& needsRerender) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (selected != nullptr && ImGui::Button("Deselect", ImVec2(avail.x, 0))) {
        selected = nullptr;
        needsRerender = true;
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Scene Objects");

    std::vector<ST::GameObject*> rootObjects = scene.getRootObjects();
    if (rootObjects.empty()) {
        ImGui::TextDisabled("No objects");
        return;
    }
    std::stable_sort(rootObjects.begin(), rootObjects.end(),
        [](const ST::GameObject* a, const ST::GameObject* b) {
            return a->sortingOrder < b->sortingOrder;
        });

    appendSceneHierarchyEntries(scene, rootObjects, selected, needsRerender);
}

void TestModule_2D_Scene::renderScene(ST::FrameBuffer* fb, int renderW, int renderH) {
    static const int kGameResolutions[][2] = {
        { 640, 480 },
        { 800, 600 },
        { 1024, 768 },
        { 1280, 720 },
        { 1280, 800 },
        { 1366, 768 },
        { 1440, 900 },
        { 1600, 900 },
        { 1680, 1050 },
        { 1920, 1080 },
        { 1920, 1200 },
        { 2560, 1080 },
        { 2560, 1440 },
        { 3440, 1440 },
        { 3840, 2160 }
    };
    static constexpr int kGameResolutionCount = sizeof(kGameResolutions) / sizeof(kGameResolutions[0]);

    ST::Matrix4x4 view, projection;

    if (m_viewportMode == 0) {
        // Scene mode — clear first, then choose 2D or 3D view based on m_view3D.
        fb->clear(ST::Color(0.1f, 0.1f, 0.1f, 1.0f));

        if (m_view3D) {
            // Pull latest orbit camera state into the Scene camera before drawing.
            m_orbit.applyTo(m_scene.camera);
            // Keep aspect in sync with current render target so projection isn't stretched.
            m_scene.camera.m_aspect = (renderH > 0)
                ? (float)renderW / (float)renderH
                : m_scene.camera.m_aspect;
            view = m_scene.camera.getViewMatrix();
            projection = m_scene.camera.getProjectionMatrix();
        } else {
            view = ST::Matrix4x4::translation(m_panX, m_panY, 0.0f) *
                   ST::Matrix4x4::scale(m_zoom, m_zoom, 1.0f);
            float halfW = m_canvasW * 0.5f / m_zoom;
            float halfH = m_canvasH * 0.5f / m_zoom;
            projection = ST::Matrix4x4::orthographic(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
        }
    } else {
        // Game mode: render into a centered sub-viewport, unused area uses a darker color
        const ST::Color outerBg(0.08f, 0.08f, 0.08f, 1.0f);
        const ST::Color gameBg(0.15f, 0.15f, 0.15f, 1.0f);
        fb->clear(outerBg);

        const int safeIndex = std::clamp(m_gameResolutionIndex, 0, kGameResolutionCount - 1);
        const float outputWidth = static_cast<float>(kGameResolutions[safeIndex][0]);
        const float outputHeight = static_cast<float>(kGameResolutions[safeIndex][1]);
        const float outputAspect = outputWidth / outputHeight;

        static constexpr float kGameViewportWidthScale = 0.95f;
        int gameViewportW = std::max(1, static_cast<int>(std::round(static_cast<float>(renderW) * kGameViewportWidthScale)));
        int gameViewportH = std::max(1, static_cast<int>(std::round(static_cast<float>(gameViewportW) / outputAspect)));
        if (gameViewportH > renderH) {
            gameViewportH = renderH;
            gameViewportW = std::max(1, static_cast<int>(std::round(static_cast<float>(renderH) * outputAspect)));
        }
        const int gameViewportX = (renderW - gameViewportW) / 2;
        const int gameViewportY = (renderH - gameViewportH) / 2;

        for (int y = gameViewportY; y < gameViewportY + gameViewportH; y++) {
            for (int x = gameViewportX; x < gameViewportX + gameViewportW; x++) {
                fb->setPixel(x, y, gameBg);
            }
        }

        ST::Cinemachine* mainCam = m_scene.findCinemachine();
        view = ST::Matrix4x4::identity();
        if (mainCam) {
            const float gameWorldWidth = mainCam->getViewportWidth();
            const float gameWorldHeight = gameWorldWidth / outputAspect;
            const float gameHalfW = gameWorldWidth * 0.5f;
            const float gameHalfH = gameWorldHeight * 0.5f;

            projection = ST::Matrix4x4::orthographic(
                mainCam->position.x - gameHalfW, mainCam->position.x + gameHalfW,
                mainCam->position.y - gameHalfH, mainCam->position.y + gameHalfH,
                -1.0f, 1.0f);
        } else {
            projection = ST::Matrix4x4::orthographic(-1, 1, -1, 1, -1, 1);
        }

        m_rasterizer.setBuffers(fb, nullptr);
        m_rasterizer.setViewport(gameViewportX, gameViewportY, gameViewportW, gameViewportH);

        // Draw game viewport border (white) - bottom layer
        int borderThick = 2;
        ST::Color white(1, 1, 1, 1);
        for (int t = 0; t < borderThick; t++) {
            for (int x = gameViewportX + t; x < gameViewportX + gameViewportW - t; x++) {
                fb->setPixel(x, gameViewportY + t, white);
                fb->setPixel(x, gameViewportY + gameViewportH - 1 - t, white);
            }
            for (int y = gameViewportY + t; y < gameViewportY + gameViewportH - t; y++) {
                fb->setPixel(gameViewportX + t, y, white);
                fb->setPixel(gameViewportX + gameViewportW - 1 - t, y, white);
            }
        }
    }

    m_vertexShader.uniforms.viewMatrix = view;
    m_vertexShader.uniforms.projectionMatrix = projection;

    std::vector<ST::GameObject*> sortedObjs;
    m_scene.getSortedObjects(sortedObjs);

    if (m_viewportMode == 0) {
        m_rasterizer.setBuffers(fb, nullptr);
    }
    m_rasterizer.setUseRawScreenCoords(false);

    const bool use3DModel = (m_viewportMode == 0 && m_view3D);

    for (auto* obj : sortedObjs) {
        if (dynamic_cast<ST::Cinemachine*>(obj)) continue;

        ST::Matrix4x4 model = use3DModel
            ? buildWorldMatrix3D(obj)
            : buildWorldMatrix(obj);
        m_vertexShader.uniforms.modelMatrix = model;

        const auto& vertices = obj->getMesh().getVertices();
        const auto& indices = obj->getMesh().getIndices();

        ST::Sprite2D* sprite = dynamic_cast<ST::Sprite2D*>(obj);
        bool useTexture = sprite && sprite->hasValidTexture();

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            ST::VertexOut v0 = m_vertexShader.process(vertices[indices[i]]);
            ST::VertexOut v1 = m_vertexShader.process(vertices[indices[i + 1]]);
            ST::VertexOut v2 = m_vertexShader.process(vertices[indices[i + 2]]);

            if (useTexture) {
                m_rasterizer.rasterizeTriangle(v0, v1, v2,
                    [&](const ST::VertexOut& frag) {
                        float u = frag.texCoord.x;
                        float v = frag.texCoord.y;
                        if (sprite->getFlipX()) u = 1.0f - u;
                        if (sprite->getFlipY()) v = 1.0f - v;
                        ST::Color texColor = sprite->getTexture().sample(u, v);
                        return ST::Color(texColor.r * sprite->tint.r,
                                         texColor.g * sprite->tint.g,
                                         texColor.b * sprite->tint.b,
                                         texColor.a * sprite->tint.a);
                    });
            } else {
                ST::Color objColor = obj->color;
                m_rasterizer.rasterizeTriangle(v0, v1, v2,
                    [&](const ST::VertexOut& frag) {
                        if (use3DModel) {
                            ST::Vector3 camForward = (m_scene.camera.m_target - m_scene.camera.m_eye).normalized();
                            float shade = 0.55f + 0.45f * std::max(0.0f, frag.normal.normalized().dot(camForward));
                            return ST::Color(objColor.r * shade,
                                             objColor.g * shade,
                                             objColor.b * shade,
                                             objColor.a);
                        }
                        return objColor;
                    });
            }
        }
    }

    // Selection AABB (cyan) drawn in screen space.
    // In 3D mode this is replaced by a world-space wireframe overlay (computed below).
    if (m_selected && !use3DModel && m_selected->showAABB) {
        ST::Matrix4x4 model = buildWorldMatrix(m_selected);

        const auto& vertices = m_selected->getMesh().getVertices();

        float minX = FLT_MAX, maxX = -FLT_MAX;
        float minY = FLT_MAX, maxY = -FLT_MAX;

        for (const auto& v : vertices) {
            ST::Vector4 world = model * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            float wx = world.x / world.w;
            float wy = world.y / world.w;
            if (wx < minX) minX = wx;
            if (wx > maxX) maxX = wx;
            if (wy < minY) minY = wy;
            if (wy > maxY) maxY = wy;
        }

        ST::Vector4 corners[4] = {
            ST::Vector4(minX, minY, 0.0f, 1.0f),
            ST::Vector4(maxX, minY, 0.0f, 1.0f),
            ST::Vector4(maxX, maxY, 0.0f, 1.0f),
            ST::Vector4(minX, maxY, 0.0f, 1.0f),
        };

        int sc[4][2];
        for (int i = 0; i < 4; i++) {
            ST::Vector4 clip = projection * view * corners[i];
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            sc[i][0] = (int)((ndcX + 1.0f) * 0.5f * renderW);
            sc[i][1] = (int)((1.0f - ndcY) * 0.5f * renderH);
        }

        ST::Color aabbCol(1.0f, 0.0f, 0.0f, 1.0f);
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            drawLine(fb, sc[i][0], sc[i][1], sc[j][0], sc[j][1], aabbCol);
        }
    }

    if (m_selected && m_viewportMode == 0 && !use3DModel) {
        ST::Matrix4x4 model = buildWorldMatrix(m_selected);

        const auto& vertices = m_selected->getMesh().getVertices();

        float minX = FLT_MAX, maxX = -FLT_MAX;
        float minY = FLT_MAX, maxY = -FLT_MAX;

        for (const auto& v : vertices) {
            ST::Vector4 world = model * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            float wx = world.x / world.w;
            float wy = world.y / world.w;
            if (wx < minX) minX = wx;
            if (wx > maxX) maxX = wx;
            if (wy < minY) minY = wy;
            if (wy > maxY) maxY = wy;
        }

        float pad = 2.0f / m_zoom;
        ST::Vector4 corners[4] = {
            ST::Vector4(minX - pad, minY - pad, 0.0f, 1.0f),
            ST::Vector4(maxX + pad, minY - pad, 0.0f, 1.0f),
            ST::Vector4(maxX + pad, maxY + pad, 0.0f, 1.0f),
            ST::Vector4(minX - pad, maxY + pad, 0.0f, 1.0f),
        };

        int sc[4][2];
        for (int i = 0; i < 4; i++) {
            ST::Vector4 clip = projection * view * corners[i];
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            sc[i][0] = (int)((ndcX + 1.0f) * 0.5f * renderW);
            sc[i][1] = (int)((1.0f - ndcY) * 0.5f * renderH);
        }

        ST::Color selectCol(0.0f, 1.0f, 1.0f, 1.0f);
        for (int i = 0; i < 4; i++) {
            int j = (i + 1) % 4;
            drawLine(fb, sc[i][0], sc[i][1], sc[j][0], sc[j][1], selectCol);
        }
    }

    if (m_showGrid && m_viewportMode == 0 && !use3DModel) {
        renderGridQuad(fb, view, projection, m_canvasW, m_canvasH);
    }

    if (m_viewportMode == 0 && !use3DModel) {
        if (ST::Cinemachine* cm = m_scene.findCinemachine()) {
            float hw = cm->getViewportWidth() * 0.5f;
            float hh = cm->getViewportHeight() * 0.5f;
            ST::Vector4 worldCorners[4] = {
                ST::Vector4(cm->position.x - hw, cm->position.y - hh, 0.0f, 1.0f),
                ST::Vector4(cm->position.x + hw, cm->position.y - hh, 0.0f, 1.0f),
                ST::Vector4(cm->position.x + hw, cm->position.y + hh, 0.0f, 1.0f),
                ST::Vector4(cm->position.x - hw, cm->position.y + hh, 0.0f, 1.0f),
            };
            int sc[4][2];
            for (int i = 0; i < 4; i++) {
                ST::Vector4 clip = projection * view * worldCorners[i];
                sc[i][0] = (int)((clip.x / clip.w * 0.5f + 0.5f) * renderW);
                sc[i][1] = (int)((-clip.y / clip.w * 0.5f + 0.5f) * renderH);
            }
            ST::Color borderCol(1.0f, 1.0f, 1.0f, 1.0f);
            for (int thickness = 0; thickness < 2; thickness++) {
                for (int i = 0; i < 4; i++) {
                    int j = (i + 1) % 4;
                    drawLine(fb, sc[i][0] + thickness, sc[i][1], sc[j][0] + thickness, sc[j][1], borderCol);
                }
                for (int i = 0; i < 4; i++) {
                    int j = (i + 1) % 4;
                    drawLine(fb, sc[i][0], sc[i][1] + thickness, sc[j][0], sc[j][1] + thickness, borderCol);
                }
            }
        }
    }

    // 3D selection highlight: a world-space box around selected's mesh vertices.
    if (m_selected && use3DModel) {
        ST::Matrix4x4 model = buildWorldMatrix3D(m_selected);
        const auto& verts = m_selected->getMesh().getVertices();
        if (!verts.empty()) {
            float minX = FLT_MAX, minY = FLT_MAX, minZ = FLT_MAX;
            float maxX = -FLT_MAX, maxY = -FLT_MAX, maxZ = -FLT_MAX;
            for (const auto& v : verts) {
                ST::Vector4 wp = model * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
                float w = 1.0f / wp.w;
                if (wp.x * w < minX) minX = wp.x * w;
                if (wp.x * w > maxX) maxX = wp.x * w;
                if (wp.y * w < minY) minY = wp.y * w;
                if (wp.y * w > maxY) maxY = wp.y * w;
                if (wp.z * w < minZ) minZ = wp.z * w;
                if (wp.z * w > maxZ) maxZ = wp.z * w;
            }
            ST::Vector3 corners[8] = {
                ST::Vector3(minX, minY, minZ), ST::Vector3(maxX, minY, minZ),
                ST::Vector3(maxX, maxY, minZ), ST::Vector3(minX, maxY, minZ),
                ST::Vector3(minX, minY, maxZ), ST::Vector3(maxX, minY, maxZ),
                ST::Vector3(maxX, maxY, maxZ), ST::Vector3(minX, maxY, maxZ),
            };
            int edges[12][2] = {
                {0,1},{1,2},{2,3},{3,0}, // -Z face
                {4,5},{5,6},{6,7},{7,4}, // +Z face
                {0,4},{1,5},{2,6},{3,7}  // connectors
            };
            ST::Color selectCol(0.0f, 1.0f, 1.0f, 1.0f);
            for (int e = 0; e < 12; ++e) {
                ST::Vector4 f0 = projection * (view * ST::Vector4(corners[edges[e][0]], 1.0f));
                ST::Vector4 f1 = projection * (view * ST::Vector4(corners[edges[e][1]], 1.0f));
                int sx0 = (int)((f0.x / f0.w * 0.5f + 0.5f) * renderW);
                int sy0 = (int)((-f0.y / f0.w * 0.5f + 0.5f) * renderH);
                int sx1 = (int)((f1.x / f1.w * 0.5f + 0.5f) * renderW);
                int sy1 = (int)((-f1.y / f1.w * 0.5f + 0.5f) * renderH);
                drawLine(fb, sx0, sy0, sx1, sy1, selectCol);
            }
        }
    }
}

void TestModule_2D_Scene::render(void* canvasTexture, int canvasW, int canvasH) {
    m_canvasW = canvasW;
    m_canvasH = canvasH;

    static int lastMode = -1;
    if (lastMode != m_viewportMode) {
        lastMode = m_viewportMode;
        if (m_viewportMode == 0 && m_gameRunning) {
            m_gameRunning = false;
        }
        syncGameRuntimeState();
        if (m_viewportMode == 1) {
            m_panX = m_scene.viewportOffset.x;
            m_panY = m_scene.viewportOffset.y;
            m_zoom = m_scene.viewportZoom;
        }
    }

    updateCinemachine(1.0f / 60.0f);

    if (m_viewportMode == 0) {
        m_scene.viewportOffset.x = m_panX;
        m_scene.viewportOffset.y = m_panY;
        m_scene.viewportZoom = m_zoom;
    } else {
        m_dragging = false;
        m_objDragging = false;
        m_panX = m_scene.viewportOffset.x;
        m_panY = m_scene.viewportOffset.y;
        m_zoom = m_scene.viewportZoom;
    }

    bool gameModeHasCamera = false;
    if (m_viewportMode == 1) {
        gameModeHasCamera = (m_scene.findCinemachine() != nullptr);
    }
    bool doRender = (m_viewportMode == 0) || gameModeHasCamera;

    rebuildBuffers(canvasW, canvasH);

    if (doRender) {
        if (m_aaMode == 1) {
            rebuildSSAABuffer(canvasW, canvasH);
            int ssaaW = canvasW * m_ssaaLevel;
            int ssaaH = canvasH * m_ssaaLevel;
            renderScene(m_ssaaBuffer, ssaaW, ssaaH);
            downsample(m_ssaaBuffer, m_frameBuffer);
        } else {
            renderScene(m_frameBuffer, canvasW, canvasH);
        }
        if (m_aaMode == 2) {
            applyFXAA(m_frameBuffer);
        }
    } else {
        m_frameBuffer->clear(ST::Color(0.0f, 0.0f, 0.0f, 1.0f));
    }

    // Upload framebuffer to GPU texture for display
    const auto& pixels = m_frameBuffer->getPixels();
    std::vector<uint32_t> sdlPixels(canvasW * canvasH);
    for (int i = 0; i < canvasW * canvasH; i++) {
        uint8_t r = (uint8_t)(pixels[i].r * 255);
        uint8_t g = (uint8_t)(pixels[i].g * 255);
        uint8_t b = (uint8_t)(pixels[i].b * 255);
        uint8_t a = (uint8_t)(pixels[i].a * 255);
        sdlPixels[i] = r | (g << 8) | (b << 16) | (a << 24);
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
        (void*)sdlPixels.data(), canvasW, canvasH, 32, canvasW * 4,
        0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
    SDL_Texture* displayTex = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);

    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, displayTex, nullptr, nullptr);
    SDL_DestroyTexture(displayTex);
}

void TestModule_2D_Scene::renderGridQuad(ST::FrameBuffer* fb, const ST::Matrix4x4& view, const ST::Matrix4x4& projection, int canvasW, int canvasH) {
    ST::Matrix4x4 invVP = (projection * view).inverse();

    for (int ty = 0; ty < canvasH; ty += 1) {
        for (int tx = 0; tx < canvasW; tx += 1) {
            float ndcX = (tx + 0.5f) / canvasW * 2.0f - 1.0f;
            float ndcY = 1.0f - (ty + 0.5f) / canvasH * 2.0f;

            ST::Vector4 worldPos = invVP * ST::Vector4(ndcX, ndcY, 0.0f, 1.0f);
            float wx = worldPos.x / worldPos.w;
            float wy = worldPos.y / worldPos.w;

            float gx = wrapFraction(wx / m_gridSize);
            float gy = wrapFraction(wy / m_gridSize);

            float lineX = std::min(gx, 1.0f - gx);
            float lineY = std::min(gy, 1.0f - gy);
            float line = std::min(lineX, lineY);

            float thickness = 0.008f;
            float gridAlpha = 0.0f;
            if (line < thickness) {
                gridAlpha = 1.0f - line / thickness;
            }

            bool isAxisX = std::abs(wrapFraction(wx / m_gridSize + 0.5f) - 0.5f) < thickness * 2.0f;
            bool isAxisY = std::abs(wrapFraction(wy / m_gridSize + 0.5f) - 0.5f) < thickness * 2.0f;
            if (isAxisX || isAxisY) {
                gridAlpha = 1.0f;
            }

            if (gridAlpha > 0.0f) {
                float gray = (isAxisX || isAxisY) ? 0.5f : 0.2f;
                ST::Color existing = fb->getPixel(tx, ty);
                float a = gridAlpha;
                ST::Color blended(existing.r * (1 - a) + gray * a,
                                existing.g * (1 - a) + gray * a,
                                existing.b * (1 - a) + gray * a,
                                1.0f);
                fb->setPixel(tx, ty, blended);
            }
        }
    }
}

void TestModule_2D_Scene::drawLine(ST::FrameBuffer* fb, int x0, int y0, int x1, int y1, ST::Color color) {
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        if (x0 >= 0 && x0 < fb->getWidth() &&
            y0 >= 0 && y0 < fb->getHeight()) {
            fb->setPixel(x0, y0, color);
        }

        if (x0 == x1 && y0 == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void TestModule_2D_Scene::runConsole(std::string& output) {
    output = "[2D Scene] Running.\n";
    output += "Zoom: " + std::to_string(m_zoom) + "\n";
    output += "Pan: (" + std::to_string(m_panX) + ", " + std::to_string(m_panY) + ")\n";
    output += "Grid size: " + std::to_string(m_gridSize) + " units\n";
    output += "Objects: " + std::to_string(m_scene.getObjectCount()) + "\n";
    output += "SSAA: " + std::string(m_ssaaEnabled ? "ON" : "OFF") + "\n";
    if (m_selected) {
        output += "Selected: " + m_selected->name + "\n";
        output += "Position: (" + std::to_string(m_selected->position.x) + ", " + std::to_string(m_selected->position.y) + ")\n";
        output += "Rotation: " + std::to_string(m_selected->rotation.z * 180.0f / 3.14159f) + " deg\n";
        output += "Scale: (" + std::to_string(m_selected->scale.x) + ", " + std::to_string(m_selected->scale.y) + ")\n";
    }
}

bool TestModule_2D_Scene::renderControls() {
    static const char* kGameResolutionLabels =
        "640x480 (4:3)\0"
        "800x600 (4:3)\0"
        "1024x768 (4:3)\0"
        "1280x720 (16:9)\0"
        "1280x800 (16:10)\0"
        "1366x768 (16:9)\0"
        "1440x900 (16:10)\0"
        "1600x900 (16:9)\0"
        "1680x1050 (16:10)\0"
        "1920x1080 (16:9)\0"
        "1920x1200 (16:10)\0"
        "2560x1080 (21:9)\0"
        "2560x1440 (16:9)\0"
        "3440x1440 (21:9)\0"
        "3840x2160 (16:9)\0";

    ImGui::Separator();
    ImGui::Text("2D Scene Controls");
    ImGui::Separator();
    bool changed = false;
    if (m_viewportMode == 0) {
        ImGui::Checkbox("3D View", &m_view3D);
        if (m_view3D) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset Cam")) {
                resetOrbitCamera();
                changed = true;
            }
        }
    }
    if (m_view3D && m_viewportMode == 0) {
        ImGui::Text("3D Controls");
        ImGui::BulletText("RMB drag: orbit camera");
        ImGui::BulletText("W/A/S/D: pan target");
        ImGui::BulletText("Q/E: pan down/up");
        ImGui::BulletText("Wheel: zoom");
        ImGui::BulletText("F: focus selected");
        ImGui::BulletText("R: reset camera");
        ImGui::BulletText("V: toggle 2D/3D view");
    } else {
        ImGui::Text("RMB: Pan view");
        ImGui::Text("Wheel: Zoom");
        ImGui::Text("LMB: Select & drag object");
        if (m_viewportMode == 0) ImGui::Text("V: toggle 2D/3D view");
    }
    ImGui::Spacing();
    ImGui::Separator();

    if (m_view3D && m_viewportMode == 0) {
        ImGui::Text("3D Camera");
        float fovDeg = m_scene.camera.m_fov * 180.0f / 3.14159f;
        if (ImGui::SliderFloat("FOV", &fovDeg, 10.0f, 120.0f)) {
            m_scene.camera.m_fov = fovDeg * 3.14159f / 180.0f;
            changed = true;
        }
        float nearP = m_scene.camera.m_near;
        float farP  = m_scene.camera.m_far;
        if (ImGui::DragFloat("Near", &nearP, 0.1f, 0.001f, 100.0f)) {
            m_scene.camera.m_near = std::max(0.001f, nearP);
            changed = true;
        }
        if (ImGui::DragFloat("Far", &farP, 1.0f, 100.0f, 50000.0f)) {
            m_scene.camera.m_far = std::max(m_scene.camera.m_near + 0.1f, farP);
            changed = true;
        }
        ImGui::Text("Mode: %s", m_scene.camera.m_mode == ST::ProjectionMode::Perspective ? "Perspective" : "Orthographic");
        if (ImGui::Button("Toggle Mode")) {
            m_scene.camera.m_mode = (m_scene.camera.m_mode == ST::ProjectionMode::Perspective)
                ? ST::ProjectionMode::Orthographic : ST::ProjectionMode::Perspective;
            changed = true;
        }
        ImGui::Separator();
    }

    ImGui::Text("Rendering");
    changed |= ImGui::Combo("Anti-Aliasing", &m_aaMode, "None\0SSAA (2x)\0FXAA\0");
    ImGui::Checkbox("Show Grid", &m_showGrid);
    if (m_viewportMode == 1) {
        if (m_gameRunning) {
            if (ImGui::Button("Stop Game", ImVec2(-1, 0))) {
                m_gameRunning = false;
                syncGameRuntimeState();
                changed = true;
            }
        } else {
            if (ImGui::Button("Start Game", ImVec2(-1, 0))) {
                m_gameRunning = true;
                syncGameRuntimeState();
                changed = true;
            }
        }
        ImGui::Text("Game Output Resolution");
        changed |= ImGui::Combo("##game_resolution", &m_gameResolutionIndex, kGameResolutionLabels);
    }

    ImGui::Spacing();
    ImGui::Separator();

    bool uiChanged = false;
    if (m_selected) {
        if (dynamic_cast<ST::Cinemachine*>(m_selected)) {
            ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Cinemachine");
            ImGui::Separator();
            uiChanged = m_cinemachineUI.renderControls(m_selected);
        } else {
            uiChanged = m_scene.renderTransformControls(m_selected);
        }
    }

    if (ImGui::Button("Reset View")) {
        m_zoom = 1.0f;
        m_panX = 0.0f;
        m_panY = 0.0f;
        updateViewFromCamera();
        changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Scene File");
    ImGui::Separator();

    if (ImGui::Button("Save Scene", ImVec2(-1, 0))) {
        IFileDialog* pfd = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
        if (SUCCEEDED(hr)) {
            COMDLG_FILTERSPEC specs[] = { {L"Scene Files", L"*.scene"}, {L"All Files", L"*.*"} };
            pfd->SetFileTypes(2, specs);
            IShellItem* psiResult = nullptr;
            hr = pfd->Show(nullptr);
            if (SUCCEEDED(hr)) {
                hr = pfd->GetResult(&psiResult);
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = nullptr;
                    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr)) {
                        char buf[MAX_PATH];
                        WideCharToMultiByte(CP_ACP, 0, pszPath, -1, buf, MAX_PATH, nullptr, nullptr);
                        if (ST::SceneData::saveScene(m_scene, std::string(buf))) {
                            m_lastSavePath = buf;
                        }
                        CoTaskMemFree(pszPath);
                    }
                    psiResult->Release();
                }
            }
            pfd->Release();
        }
    }

    if (ImGui::Button("Load Scene", ImVec2(-1, 0))) {
        IFileDialog* pfd = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
        if (SUCCEEDED(hr)) {
            COMDLG_FILTERSPEC specs[] = { {L"Scene Files", L"*.scene"}, {L"All Files", L"*.*"} };
            pfd->SetFileTypes(2, specs);
            IShellItem* psiResult = nullptr;
            hr = pfd->Show(nullptr);
            if (SUCCEEDED(hr)) {
                hr = pfd->GetResult(&psiResult);
                if (SUCCEEDED(hr)) {
                    PWSTR pszPath = nullptr;
                    hr = psiResult->GetDisplayName(SIGDN_FILESYSPATH, &pszPath);
                    if (SUCCEEDED(hr)) {
                        char buf[MAX_PATH];
                        WideCharToMultiByte(CP_ACP, 0, pszPath, -1, buf, MAX_PATH, nullptr, nullptr);
                        ST::Scene scene;
                        if (ST::SceneData::loadScene(scene, std::string(buf))) {
                            m_scene = std::move(scene);
                            m_scene.setTransformUI(&m_transformUI);
                            m_selected = nullptr;
                            needsRerender = true;
                            changed = true;
                        }
                        CoTaskMemFree(pszPath);
                    }
                    psiResult->Release();
                }
            }
            pfd->Release();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();

    return changed || uiChanged;
}

void TestModule_2D_Scene::renderCreatePanel() {
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Create Object");
    ImGui::Separator();

    const char* typeNames[] = { "Quad", "Circle", "Sprite2D", "MusicPlayer", "CinemachineTarget", "Cube3D" };
    static int curType = 0;
    ImGui::Combo("Type", &curType, typeNames, 6);

    ImGui::Spacing();
    if (ImGui::Button("Spawn at Origin", ImVec2(-1, 0))) {
        char name[64];
        snprintf(name, sizeof(name), "%s_%zu", typeNames[curType], m_scene.getObjectCount());
        ST::GameObject* obj = nullptr;
        if (curType == 0) {
            obj = m_scene.createGameObject(name);
            obj->createQuad(100.0f, 100.0f);
        } else if (curType == 1) {
            obj = m_scene.createGameObject(name);
            obj->createCircle(50.0f, 32);
        } else if (curType == 2) {
            obj = m_scene.createSprite2D(name);
        } else if (curType == 3) {
            obj = m_scene.createMusicPlayer(name);
        } else if (curType == 4) {
            obj = m_scene.createCinemachine(name);
            static_cast<ST::Cinemachine*>(obj)->setViewportWidth(640.0f);
            obj->setColor(1.0f, 1.0f, 1.0f, 1.0f);
        } else if (curType == 5) {
            obj = m_scene.createGameObject(name);
            obj->createCube(100.0f, 100.0f, 100.0f);
            obj->setColor(0.8f, 0.8f, 0.8f);
        }
        if (obj) {
            obj->setScale(1.0f, 1.0f, 1.0f);
            obj->setPosition(0.0f, 0.0f, 0.0f);
            obj->setColor(1.0f, 1.0f, 1.0f);
            if (m_selected) {
                obj->setParent(m_selected);
            }
            m_selected = obj;
            needsRerender = true;
        }
    }

    ImGui::Separator();

    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Scene Objects");
    ImGui::Separator();

    renderHierarchyPanel(m_scene, m_selected, needsRerender);
}

void TestModule_2D_Scene::renderUIOverlay(int, int, int, int) {
    // UI rendering is now done inside render() via GPU composite
}

void TestModule_2D_Scene::renderOverlays() {
    m_transformUI.renderBrowserOnTop();
    m_transformUI.renderAudioBrowserOnTop();
}

void TestModule_2D_Scene::buildThumbnails(SDL_Renderer* renderer) {
    for (auto* t : m_thumbnails) {
        if (t) SDL_DestroyTexture(t);
    }
    m_thumbnails.clear();

    const auto& textures = ST::TextureManager::getInstance().getAllTextures();
    for (const auto& texInfo : textures) {
        if (!texInfo.texture.isValid()) {
            m_thumbnails.push_back(nullptr);
            continue;
        }

        const auto& pixels = texInfo.texture.getPixels();
        int w = texInfo.texture.getWidth();
        int h = texInfo.texture.getHeight();

        std::vector<uint8_t> rgba(w * h * 4);
        for (int i = 0; i < w * h; i++) {
            rgba[i * 4 + 0] = static_cast<uint8_t>(std::clamp(pixels[i].r, 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 1] = static_cast<uint8_t>(std::clamp(pixels[i].g, 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 2] = static_cast<uint8_t>(std::clamp(pixels[i].b, 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 3] = static_cast<uint8_t>(std::clamp(pixels[i].a, 0.0f, 1.0f) * 255.0f);
        }

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(
            rgba.data(), w, h, 32,
            w * 4, SDL_PIXELFORMAT_RGBA32);
        if (!surf) {
            m_thumbnails.push_back(nullptr);
            continue;
        }

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);

        if (!tex) {
            m_thumbnails.push_back(nullptr);
            continue;
        }

        m_thumbnails.push_back(tex);
    }
}

void TestModule_2D_Scene::setRenderer(void* renderer) {
    if ((SDL_Renderer*)renderer != m_renderer) {
        m_renderer = (SDL_Renderer*)renderer;
        if (m_renderer) buildThumbnails(m_renderer);
    }
    m_transformUI.setThumbnails(m_renderer, m_thumbnails);
    m_transformUI.setTextureCallback([this](int) {
        needsRerender = true;
    });
}

// Pick the topmost object under (canvasX, canvasY) in 3D view by ray-casting.
// Returns the world-space ray and tests against AABB in object-local space per GameObject.
ST::GameObject* TestModule_2D_Scene::pickObject3D(int canvasX, int canvasY, int canvasW, int canvasH) {
    if (canvasW <= 0 || canvasH <= 0) return nullptr;

    // Convert canvas pixel to NDC.
    float ndcX = (canvasX + 0.5f) / (float)canvasW * 2.0f - 1.0f;
    float ndcY = 1.0f - (canvasY + 0.5f) / (float)canvasH * 2.0f;

    // Forward camera state.
    m_orbit.applyTo(m_scene.camera);
    float aspect = (canvasH > 0) ? (float)canvasW / (float)canvasH : 1.0f;
    m_scene.camera.m_aspect = aspect;
    ST::Matrix4x4 view = m_scene.camera.getViewMatrix();
    ST::Matrix4x4 proj = m_scene.camera.getProjectionMatrix();
    ST::Matrix4x4 invVP = (proj * view).inverse();

    // Compute two world points: near and far along the click ray.
    ST::Vector4 nearH = invVP * ST::Vector4(ndcX, ndcY, -1.0f, 1.0f);
    ST::Vector4 farH  = invVP * ST::Vector4(ndcX, ndcY,  1.0f, 1.0f);
    ST::Vector3 rayOrigin(
        nearH.x / nearH.w,
        nearH.y / nearH.w,
        nearH.z / nearH.w);
    ST::Vector3 rayEnd(
        farH.x / farH.w,
        farH.y / farH.w,
        farH.z / farH.w);
    ST::Vector3 rayDir = (rayEnd - rayOrigin);
    if (rayDir.lengthSquared() < 1e-12f) return nullptr;
    rayDir.normalize();

    // Slab test in world space using each object's transformed mesh AABB.
    ST::GameObject* best = nullptr;
    float bestT = 1e30f;

    std::vector<ST::GameObject*> objs;
    m_scene.getSortedObjects(objs);
    for (ST::GameObject* obj : objs) {
        if (dynamic_cast<ST::Cinemachine*>(obj)) continue;
        const auto& verts = obj->getMesh().getVertices();
        if (verts.empty()) continue;
        ST::Matrix4x4 model = buildWorldMatrix3D(obj);

        float mnX =  FLT_MAX, mnY =  FLT_MAX, mnZ =  FLT_MAX;
        float mxX = -FLT_MAX, mxY = -FLT_MAX, mxZ = -FLT_MAX;
        for (const auto& v : verts) {
            ST::Vector4 wp = model * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            float w = 1.0f / wp.w;
            float x = wp.x * w, y = wp.y * w, z = wp.z * w;
            if (x < mnX) mnX = x; if (x > mxX) mxX = x;
            if (y < mnY) mnY = y; if (y > mxY) mxY = y;
            if (z < mnZ) mnZ = z; if (z > mxZ) mxZ = z;
        }
        // Ray-AABB slab intersection.
        float tMin = -1e30f, tMax = 1e30f;
        ST::Vector3 origin = rayOrigin;
        for (int axis = 0; axis < 3; ++axis) {
            float o = ((const float*)&origin)[axis];
            float d = ((const float*)&rayDir)[axis];
            float lo_, hi_;
            if      (axis == 0) { lo_ = mnX; hi_ = mxX; }
            else if (axis == 1) { lo_ = mnY; hi_ = mxY; }
            else                { lo_ = mnZ; hi_ = mxZ; }
            if (std::abs(d) < 1e-8f) {
                if (o < lo_ || o > hi_) { tMin = 1e30f; tMax = -1e30f; break; }
            } else {
                float t1 = (lo_ - o) / d;
                float t2 = (hi_ - o) / d;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) break;
            }
        }
        if (tMin > tMax || tMax < 0.0f) continue;
        float tHit = std::max(0.0f, tMin);
        if (tHit < bestT) {
            bestT = tHit;
            best = obj;
        }
    }
    return best;
}
