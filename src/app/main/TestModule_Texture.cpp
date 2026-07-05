#include "TestModule_Texture.hpp"
#include "core/texture/ST_Image.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <Windows.h>


TestModule_Texture::TestModule_Texture()
    : m_frameBuffer(nullptr)
    , m_depthBuffer(nullptr)
    , m_texture(nullptr)
    , m_canvasW(0)
    , m_canvasH(0)
    , m_tint(1.0f, 1.0f, 1.0f, 1.0f)
    , m_zoom(1.0f)
    , m_rotation(0.0f)
    , m_selectedTextureIdx(-1)
    , m_showTextureBrowser(false)
    , m_browserSize(600, 400)
    , m_browserPosSet(false)
{
    m_renderer = nullptr;
    {
        m_quadMesh.addVertex(ST::Vertex(ST::Vector3(-0.5f, -0.5f, 0.0f), ST::Vector3::forward(), ST::Vector2(0.0f, 1.0f)));
        m_quadMesh.addVertex(ST::Vertex(ST::Vector3( 0.5f, -0.5f, 0.0f), ST::Vector3::forward(), ST::Vector2(1.0f, 1.0f)));
        m_quadMesh.addVertex(ST::Vertex(ST::Vector3( 0.5f,  0.5f, 0.0f), ST::Vector3::forward(), ST::Vector2(1.0f, 0.0f)));
        m_quadMesh.addVertex(ST::Vertex(ST::Vector3(-0.5f,  0.5f, 0.0f), ST::Vector3::forward(), ST::Vector2(0.0f, 0.0f)));
        m_quadMesh.addTriangle(0, 1, 2);
        m_quadMesh.addTriangle(0, 2, 3);
    }
    scanResourceFolder();
}

TestModule_Texture::~TestModule_Texture() {
    for (auto* t : m_thumbnails) {
        if (t) SDL_DestroyTexture(t);
    }
    delete m_frameBuffer;
    delete m_depthBuffer;
    delete m_texture;
}

void TestModule_Texture::scanResourceFolder() {
    m_scannedTextures.clear();
    char* basePath = SDL_GetBasePath();
    if (!basePath) return;
    std::string resourceDir(basePath);
    resourceDir += "Resource";
    SDL_free(basePath);

    const char* extensions[] = { ".jpg", ".jpeg", ".png", ".bmp", ".tga" };
    std::string pattern = resourceDir + "/*";
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::string fname = findData.cFileName;
            for (const char* ext : extensions) {
                if (fname.size() >= strlen(ext) &&
                    _stricmp(fname.c_str() + fname.size() - strlen(ext), ext) == 0) {
                    ScannedTexture t;
                    t.filename = fname;
                    t.fullPath = resourceDir + "/" + fname;
                    m_scannedTextures.push_back(t);
                    break;
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    FindClose(hFind);

    if (!m_scannedTextures.empty()) {
        m_selectedTextureIdx = 0;
        loadTextureFromPath(m_scannedTextures[0].fullPath);
    }
}

void TestModule_Texture::loadTextureFromPath(const std::string& path) {
    ST::Image img;
    if (img.load(path.c_str())) {
        m_quadTexture.setPixels(img.getPixels(), img.getWidth(), img.getHeight());
        needsRerender = true;
    }
}

void TestModule_Texture::buildThumbnails(SDL_Renderer* renderer) {
    for (auto* t : m_thumbnails) {
        if (t) SDL_DestroyTexture(t);
    }
    m_thumbnails.clear();

    for (const auto& tex : m_scannedTextures) {
        ST::Image img;
        if (!img.load(tex.fullPath.c_str())) {
            m_thumbnails.push_back(nullptr);
            continue;
        }

        const auto& pixels = img.getPixels();
        int w = img.getWidth();
        int h = img.getHeight();
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
        SDL_Texture* sdlTex = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (sdlTex) SDL_SetTextureScaleMode(sdlTex, SDL_ScaleModeBest);
        m_thumbnails.push_back(sdlTex);
    }
}

void TestModule_Texture::rebuildBuffers(int canvasW, int canvasH) {
    if (!m_frameBuffer || m_frameBuffer->getWidth() != canvasW || m_frameBuffer->getHeight() != canvasH) {
        delete m_frameBuffer;
        delete m_depthBuffer;
        m_frameBuffer = new ST::FrameBuffer();
        m_frameBuffer->initialize(canvasW, canvasH);
        m_depthBuffer = new ST::DepthBuffer();
        m_depthBuffer->initialize(canvasW, canvasH);
    }
}

void TestModule_Texture::render(void* canvasTexture, int canvasW, int canvasH) {
    m_canvasW = canvasW;
    m_canvasH = canvasH;
    rebuildBuffers(canvasW, canvasH);
    m_frameBuffer->clear(ST::Color(0.05f, 0.05f, 0.05f, 1.0f));

    ST::Matrix4x4 model = ST::Matrix4x4::rotationZ(m_rotation) * ST::Matrix4x4::scale(256.0f, 256.0f, 1.0f);
    ST::Matrix4x4 view = ST::Matrix4x4::scale(m_zoom, m_zoom, 1.0f);
    float halfW = canvasW * 0.5f / m_zoom;
    float halfH = canvasH * 0.5f / m_zoom;
    ST::Matrix4x4 projection = ST::Matrix4x4::orthographic(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
    m_vertexShader.uniforms.modelMatrix = model;
    m_vertexShader.uniforms.viewMatrix = view;
    m_vertexShader.uniforms.projectionMatrix = projection;

    const auto& verts = m_quadMesh.getVertices();
    const auto& idx = m_quadMesh.getIndices();
    ST::Texture2D* tex = m_quadTexture.isValid() ? &m_quadTexture : nullptr;
    ST::Color tint = m_tint;
    m_rasterizer.setBuffers(m_frameBuffer, nullptr);
    m_rasterizer.setUseRawScreenCoords(false);

    for (size_t i = 0; i + 2 < idx.size(); i += 3) {
        ST::VertexOut v0 = m_vertexShader.process(verts[idx[i]]);
        ST::VertexOut v1 = m_vertexShader.process(verts[idx[i + 1]]);
        ST::VertexOut v2 = m_vertexShader.process(verts[idx[i + 2]]);
        m_rasterizer.rasterizeTriangle(v0, v1, v2,
            [&](const ST::VertexOut& frag) {
                ST::Color sampled(1.0f, 1.0f, 1.0f, 1.0f);
                if (tex) sampled = tex->sample(frag.texCoord.x, frag.texCoord.y);
                return ST::Color(sampled.r * tint.r, sampled.g * tint.g, sampled.b * tint.b, sampled.a * tint.a);
            });
    }

    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;
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
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_DestroyTexture(texture);
}

bool TestModule_Texture::renderControls() {
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Texture Test");
    ImGui::Separator();

    ImGui::Text("Tint Color");
    ImGui::ColorEdit4("Tint", &m_tint.r);

    ImGui::Text("Rotation: %.1f deg", m_rotation * 180.0f / 3.14159f);
    ImGui::SliderFloat("Rotation", &m_rotation, 0.0f, 6.28318f);

    ImGui::Text("Zoom: %.2fx", m_zoom);
    ImGui::SliderFloat("Zoom", &m_zoom, 0.1f, 10.0f);

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Texture: %s", m_quadTexture.isValid() ? "loaded" : "none");
    if (m_quadTexture.isValid()) {
        ImGui::Text("Size: %d x %d", m_quadTexture.getWidth(), m_quadTexture.getHeight());
        if (m_selectedTextureIdx >= 0 && m_selectedTextureIdx < (int)m_scannedTextures.size()) {
            ImGui::Text("File: %s", m_scannedTextures[m_selectedTextureIdx].filename.c_str());
        } else {
            ImGui::Text("File: external");
        }
    }

    if (ImGui::Button("Browse Textures...")) {
        m_showTextureBrowser = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%d in Resource/)", (int)m_scannedTextures.size());

    ImGui::Spacing();
    if (ImGui::Button("Open External File...")) {
        char filePath[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = filePath;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = "Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.tga\0All Files\0*.*\0";
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (GetOpenFileNameA(&ofn)) {
            loadTextureFromPath(filePath);
            m_selectedTextureIdx = -1;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(loads any image file)");

    return false;
}

void TestModule_Texture::renderOverlays() {
    if (!m_showTextureBrowser) return;

    if (!m_browserPosSet) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - m_browserSize.x * 0.5f,
                                       io.DisplaySize.y * 0.5f - m_browserSize.y * 0.5f), ImGuiCond_Once);
        m_browserPosSet = true;
    }
    ImGui::SetNextWindowSize(m_browserSize);
    if (ImGui::Begin("Texture Browser", &m_showTextureBrowser)) {

        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "Resource Textures");
        ImGui::Separator();

        if (m_scannedTextures.empty()) {
            ImGui::TextDisabled("No textures found in Resource/");
        } else {
            static char searchBuf[128] = {};
            ImGui::Text("Search:");
            ImGui::SameLine();
            ImGui::InputText("##search", searchBuf, sizeof(searchBuf));
            ImGui::Separator();

            const int thumbSize = 96;
            const int cols = 5;
            for (int i = 0; i < (int)m_scannedTextures.size(); i++) {
                if (searchBuf[0] && !strstr(m_scannedTextures[i].filename.c_str(), searchBuf)) {
                    continue;
                }
                if (i % cols != 0) ImGui::SameLine();
                bool isSelected = (i == m_selectedTextureIdx);
                std::string label = "##tex_" + std::to_string(i);
                ImVec4 tint(isSelected ? 0.3f : 1.0f, isSelected ? 0.7f : 1.0f,
                           isSelected ? 1.0f : 1.0f, 1.0f);

                bool clicked = false;
                if (m_thumbnails.size() > (size_t)i && m_thumbnails[i]) {
                    clicked = ImGui::ImageButton(label.c_str(),
                        (ImTextureID)(intptr_t)m_thumbnails[i],
                        ImVec2((float)thumbSize, (float)thumbSize),
                        ImVec2(0, 0), ImVec2(1, 1),
                        ImVec4(0, 0, 0, 1), tint);
                } else {
                    clicked = ImGui::Button(label.c_str(), ImVec2((float)thumbSize, (float)thumbSize));
                }

                if (clicked) {
                    m_selectedTextureIdx = i;
                    loadTextureFromPath(m_scannedTextures[i].fullPath);
                    m_showTextureBrowser = false;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", m_scannedTextures[i].filename.c_str());
                    ImGui::EndTooltip();
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            m_showTextureBrowser = false;
        }
        ImGui::End();
    }
}

void TestModule_Texture::setRenderer(void* renderer) {
    if ((SDL_Renderer*)renderer != m_renderer) {
        m_renderer = (SDL_Renderer*)renderer;
        buildThumbnails(m_renderer);
    }
}

void TestModule_Texture::runConsole(std::string& output) {
    output = "[Texture Test]\n";
    output += "Texture loaded: " + std::string(m_quadTexture.isValid() ? "yes" : "no") + "\n";
    if (m_quadTexture.isValid()) {
        output += "Size: " + std::to_string(m_quadTexture.getWidth()) + "x" + std::to_string(m_quadTexture.getHeight()) + "\n";
    }
    output += "Zoom: " + std::to_string(m_zoom) + "\n";
    output += "Scanned textures: " + std::to_string(m_scannedTextures.size()) + "\n";
}

void TestModule_Texture::onMouseMove(int x, int y) {}
void TestModule_Texture::onMouseDown(int button, int x, int y) {}
void TestModule_Texture::onMouseUp(int button) {}
void TestModule_Texture::onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) {}
