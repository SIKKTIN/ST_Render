#include "TestModule_Sprite2D.hpp"
#include "core/texture/ST_Image.hpp"
#include "engine/editor/TextureManager.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <imgui.h>
#include <SDL2/SDL.h>

TestModule_Sprite2D::TestModule_Sprite2D()
    : m_fb(nullptr)
    , m_canvasW(0), m_canvasH(0)
    , m_zoom(1.0f), m_panX(0.0f), m_panY(0.0f)
    , m_dragging(false), m_lastX(0), m_lastY(0)
    , m_rbtnDown(false), m_dragObjIdx(-1)
    , m_dragObjOX(0.0f), m_dragObjOY(0.0f)
    , m_selectedIdx(-1)
    , m_renderer(nullptr)
{
    m_spawnType = SpawnType::GameObject;
    m_selectedIdx = -1;
    ST::TextureManager::getInstance().scanResourceFolder();
    spawnAt(0.0f, 0.0f);
}

TestModule_Sprite2D::~TestModule_Sprite2D() {
    for (auto& e : m_objects) {
        delete e.obj;
    }
    delete m_fb;
    for (auto* t : m_thumbnails) {
        if (t) SDL_DestroyTexture(t);
    }
}

void TestModule_Sprite2D::render(void* canvasTexture, int canvasW, int canvasH) {
    if (canvasW == 0 || canvasH == 0) return;

    if (m_fb == nullptr || m_canvasW != canvasW || m_canvasH != canvasH) {
        delete m_fb;
        m_fb = new ST::FrameBuffer();
        m_fb->initialize(canvasW, canvasH);
        m_canvasW = canvasW;
        m_canvasH = canvasH;
    }

    m_fb->clear(ST::Color(0.15f, 0.15f, 0.15f, 1.0f));

    ST::Matrix4x4 view = ST::Matrix4x4::translation(m_panX, m_panY, 0.0f);
    ST::Matrix4x4 projection = ST::Matrix4x4::orthographic(
        -canvasW * 0.5f, canvasW * 0.5f,
        -canvasH * 0.5f, canvasH * 0.5f,
        -1.0f, 1.0f);

    m_rasterizer.setBuffers(m_fb, nullptr);
    m_rasterizer.setUseRawScreenCoords(false);

    for (int i = 0; i < (int)m_objects.size(); i++) {
        const auto& e = m_objects[i];
        ST::Color fill = (e.type == SpawnType::Sprite2D)
            ? ST::Color(0.8f, 0.6f, 0.2f, 1.0f)
            : ST::Color(0.2f, 0.6f, 0.8f, 1.0f);
        renderObject(e.obj, fill, i == m_selectedIdx);
    }

    const auto& pixels = m_fb->getPixels();
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;

    std::vector<uint32_t> rgba32(canvasW * canvasH);
    for (int i = 0; i < canvasW * canvasH; i++) {
        uint8_t r = static_cast<uint8_t>(std::min(255.0f, pixels[i].r * 255.0f));
        uint8_t g = static_cast<uint8_t>(std::min(255.0f, pixels[i].g * 255.0f));
        uint8_t b = static_cast<uint8_t>(std::min(255.0f, pixels[i].b * 255.0f));
        uint8_t a = static_cast<uint8_t>(std::min(255.0f, pixels[i].a * 255.0f));
        rgba32[i] = (r << 0) | (g << 8) | (b << 16) | (a << 24);
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC, canvasW, canvasH);
    SDL_UpdateTexture(tex, nullptr, rgba32.data(), canvasW * sizeof(uint32_t));
    SDL_RenderCopy(renderer, tex, nullptr, nullptr);
    SDL_DestroyTexture(tex);
}

void TestModule_Sprite2D::renderObject(const ST::GameObject* obj, ST::Color fillColor, bool selected) {
    m_vs.uniforms.viewMatrix = ST::Matrix4x4::translation(m_panX, m_panY, 0.0f);
    m_vs.uniforms.projectionMatrix = ST::Matrix4x4::orthographic(
        -m_canvasW * 0.5f, m_canvasW * 0.5f,
        -m_canvasH * 0.5f, m_canvasH * 0.5f,
        -1.0f, 1.0f);
    m_vs.uniforms.modelMatrix = buildModelMatrix(obj);

    const auto& vertices = obj->getMesh().getVertices();
    const auto& indices = obj->getMesh().getIndices();

    const ST::Sprite2D* sprite = dynamic_cast<const ST::Sprite2D*>(obj);
    bool hasTex = sprite && sprite->hasValidTexture();

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        ST::VertexOut v0 = m_vs.process(vertices[indices[i]]);
        ST::VertexOut v1 = m_vs.process(vertices[indices[i + 1]]);
        ST::VertexOut v2 = m_vs.process(vertices[indices[i + 2]]);

        v0.color = fillColor;
        v1.color = fillColor;
        v2.color = fillColor;

        if (hasTex) {
            const ST::Texture2D& tex = sprite->getTexture();
            m_rasterizer.rasterizeTriangle(v0, v1, v2,
                [&](const ST::VertexOut& frag) {
                    float u = frag.texCoord.x;
                    float v = frag.texCoord.y;
                    if (sprite->getFlipX()) u = 1.0f - u;
                    if (sprite->getFlipY()) v = 1.0f - v;
                    return tex.sample(u, v);
                });
        } else {
            m_rasterizer.rasterizeTriangle(v0, v1, v2,
                [](const ST::VertexOut& frag) { return frag.color; });
        }
    }

    if (selected) {
        ST::Matrix4x4 model = buildModelMatrix(obj);
        const auto& vx = obj->getMesh().getVertices();

        float minX = 1e6f, maxX = -1e6f;
        float minY = 1e6f, maxY = -1e6f;
        for (const auto& v : vx) {
            ST::Vector4 p = model * ST::Vector4(v.position.x, v.position.y, v.position.z, 1.0f);
            float sx = p.x / p.w;
            float sy = p.y / p.w;
            if (sx < minX) minX = sx;
            if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy;
            if (sy > maxY) maxY = sy;
        }

        ST::Vector4 corners[5] = {
            ST::Vector4(minX, minY, 0, 1), ST::Vector4(maxX, minY, 0, 1),
            ST::Vector4(maxX, maxY, 0, 1), ST::Vector4(minX, maxY, 0, 1),
            ST::Vector4(minX, minY, 0, 1),
        };
        for (int i = 0; i < 4; i++) {
            ST::Vector4 s0 = m_vs.uniforms.projectionMatrix * m_vs.uniforms.viewMatrix * corners[i];
            ST::Vector4 s1 = m_vs.uniforms.projectionMatrix * m_vs.uniforms.viewMatrix * corners[i + 1];
            int x0 = (int)((s0.x / s0.w * 0.5f + 0.5f) * m_canvasW);
            int y0 = (int)((-s0.y / s0.w * 0.5f + 0.5f) * m_canvasH);
            int x1 = (int)((s1.x / s1.w * 0.5f + 0.5f) * m_canvasW);
            int y1 = (int)((-s1.y / s1.w * 0.5f + 0.5f) * m_canvasH);

            float dx = (float)(x1 - x0);
            float dy = (float)(y1 - y0);
            float len = std::sqrt(dx * dx + dy * dy);
            int steps = (int)std::max(1.0f, len);
            for (int s = 0; s < steps; s++) {
                float t = (float)s / steps;
                m_fb->setPixel(
                    (int)(x0 + dx * t + 0.5f),
                    (int)(y0 + dy * t + 0.5f),
                    ST::Color(1, 1, 1, 1));
            }
        }
    }
}

bool TestModule_Sprite2D::hitTest(const ST::GameObject* obj, float wx, float wy) const {
    float hw = obj->scale.x * 0.5f;
    float hh = obj->scale.y * 0.5f;
    float ox = obj->position.x;
    float oy = obj->position.y;
    return wx >= ox - hw && wx <= ox + hw && wy >= oy - hh && wy <= oy + hh;
}

ST::Matrix4x4 TestModule_Sprite2D::buildModelMatrix(const ST::GameObject* obj) const {
    ST::Matrix4x4 T = ST::Matrix4x4::translation(obj->position.x, obj->position.y, obj->position.z);
    ST::Matrix4x4 S = ST::Matrix4x4::scale(obj->scale.x, obj->scale.y, 1.0f);
    return T * S;
}

void TestModule_Sprite2D::screenToWorld(int sx, int sy, int cw, int ch, float& wx, float& wy) {
    wx = (float)sx - cw * 0.5f - m_panX;
    wy = (float)sy - ch * 0.5f - m_panY;
}

bool TestModule_Sprite2D::isInCanvas(int x, int y, int cw, int ch) const {
    const float leftPanelW = 220.0f;
    const float rightPanelW = 220.0f;
    const float windowW = 1280.0f;
    return x >= leftPanelW && x < windowW - rightPanelW && y >= 0 && y < cw;
}

void TestModule_Sprite2D::spawnAt(float wx, float wy) {
    ST::GameObject* obj = nullptr;
    if (m_spawnType == SpawnType::Sprite2D) {
        obj = new ST::Sprite2D();
    } else {
        obj = new ST::GameObject();
    }
    obj->setPosition(wx, wy, 0.0f);
    obj->setScale(200.0f, 200.0f, 1.0f);
    obj->createQuad(200.0f, 200.0f);
    m_objects.push_back({ obj, m_spawnType });
    m_selectedIdx = (int)m_objects.size() - 1;
    
    needsRerender = true;
}


void TestModule_Sprite2D::buildThumbnails(SDL_Renderer* renderer) {
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
        std::vector<uint32_t> sdlPixels(w * h);
        for (int i = 0; i < w * h; i++) {
            uint8_t r = (uint8_t)(pixels[i].r * 255);
            uint8_t g = (uint8_t)(pixels[i].g * 255);
            uint8_t b = (uint8_t)(pixels[i].b * 255);
            uint8_t a = (uint8_t)(pixels[i].a * 255);
            sdlPixels[i] = r | (g << 8) | (b << 16) | (a << 24);
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            (void*)sdlPixels.data(), w, h, 32, w * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        if (!surface) {
            m_thumbnails.push_back(nullptr);
            continue;
        }
        SDL_Texture* sdlTex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (sdlTex) SDL_SetTextureScaleMode(sdlTex, SDL_ScaleModeBest);
        m_thumbnails.push_back(sdlTex);
    }
}

void TestModule_Sprite2D::setRenderer(void* renderer) {
    if ((SDL_Renderer*)renderer != m_renderer) {
        m_renderer = (SDL_Renderer*)renderer;
        if (m_renderer) buildThumbnails(m_renderer);
    }
    m_transformUI.setThumbnails(m_renderer, m_thumbnails);
    m_transformUI.setTextureCallback([this](int) {
        needsRerender = true;
    });
}

void TestModule_Sprite2D::onMouseMove(int x, int y) {
    if (m_dragging && m_dragObjIdx >= 0) {
        float wx0, wy0, wx1, wy1;
        screenToWorld(m_lastX, m_lastY, m_canvasW, m_canvasH, wx0, wy0);
        screenToWorld(x, y, m_canvasW, m_canvasH, wx1, wy1);
        m_objects[m_dragObjIdx].obj->position.x = m_dragObjOX + (wx1 - wx0);
        m_objects[m_dragObjIdx].obj->position.y = m_dragObjOY + (wy1 - wy0);
        needsRerender = true;
        m_lastX = x;
        m_lastY = y;
    }
}

void TestModule_Sprite2D::onMouseDown(int button, int x, int y) {
    if (!isInCanvas(x, y, m_canvasW, m_canvasH)) return;

    float wx, wy;
    screenToWorld(x, y, m_canvasW, m_canvasH, wx, wy);

    if (button == 1) {
        m_dragging = false;
        m_selectedIdx = -1;
        for (int i = (int)m_objects.size() - 1; i >= 0; i--) {
            if (hitTest(m_objects[i].obj, wx, wy)) {
                m_selectedIdx = i;
                m_dragging = true;
                m_dragObjIdx = i;
                m_dragObjOX = m_objects[i].obj->position.x;
                m_dragObjOY = m_objects[i].obj->position.y;
                m_lastX = x;
                m_lastY = y;
                break;
            }
        }
        needsRerender = true;
    }

    if (button == 3) {
        m_rbtnDown = true;
        m_rbtnDownX = x;
        m_rbtnDownY = y;
    }
}

void TestModule_Sprite2D::onMouseUp(int button) {
    if (button == 1) {
        m_dragging = false;
        m_dragObjIdx = -1;
    }
    if (button == 3 && m_rbtnDown) {
        if (isInCanvas(m_rbtnDownX, m_rbtnDownY, m_canvasW, m_canvasH)) {
            float wx, wy;
            screenToWorld(m_rbtnDownX, m_rbtnDownY, m_canvasW, m_canvasH, wx, wy);
            spawnAt(wx, wy);
        }
        m_rbtnDown = false;
    }
}

bool TestModule_Sprite2D::renderControls() {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Sprite2D Scene");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Objects", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_objects.empty()) {
            ImGui::TextDisabled("No objects");
        }
        for (int i = 0; i < (int)m_objects.size(); i++) {
            const char* label = (m_objects[i].type == SpawnType::Sprite2D)
                ? "Sprite2D" : "GameObject";
            if (ImGui::Selectable(label, i == m_selectedIdx)) {
                m_selectedIdx = i;
                needsRerender = true;
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Selected Object");
    ImGui::Separator();

    if (m_selectedIdx >= 0) {
        if (m_transformUI.renderControls(m_objects[m_selectedIdx].obj)) {
            needsRerender = true;
        }

        ImGui::Spacing();
        if (ImGui::Button("Delete")) {
            delete m_objects[m_selectedIdx].obj;
            m_objects.erase(m_objects.begin() + m_selectedIdx);
            if (m_selectedIdx >= (int)m_objects.size()) m_selectedIdx = -1;
            needsRerender = true;
        }
    } else {
        ImGui::TextDisabled("No object selected");
    }

    return needsRerender;
}

void TestModule_Sprite2D::renderOverlays() {
    m_transformUI.renderBrowserOnTop();
}

void TestModule_Sprite2D::renderCreatePanel() {
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Create Object");
    ImGui::Separator();
    ImGui::Text("Right-click on canvas\nto spawn object.");
    ImGui::Spacing();

    const char* typeNames[] = { "GameObject", "Sprite2D" };
    int curType = (int)m_spawnType;
    if (ImGui::Combo("Type", &curType, typeNames, 2)) {
        m_spawnType = (SpawnType)curType;
    }

    ImGui::Spacing();
    if (ImGui::Button("Spawn at Origin", ImVec2(-1, 0))) {
        spawnAt(0.0f, 0.0f);
    }
}

void TestModule_Sprite2D::runConsole(std::string& output) {
    output = "[Sprite2D] " + std::to_string(m_objects.size()) + " object(s) in scene.\n";
}
