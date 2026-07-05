#include "TestModule_VertexShader.hpp"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>

TestModule_VertexShader::TestModule_VertexShader()
    : m_frameBuffer(nullptr)
    , m_depthBuffer(nullptr)
    , m_posX(0.0f)
    , m_posY(0.0f)
    , m_rotation(0.0f)
    , m_scaleX(1.0f)
    , m_scaleY(1.0f)
    , m_renderMode(1)
    , m_showDebugBbox(true)
    , m_debugBboxMinX(0), m_debugBboxMaxX(0), m_debugBboxMinY(0), m_debugBboxMaxY(0)
{
    initMesh();

    //std::cout << "=== VertexShader Test Start!!! ===" << std::endl;
}

TestModule_VertexShader::~TestModule_VertexShader() {
    delete m_frameBuffer;
    delete m_depthBuffer;
}

void TestModule_VertexShader::initMesh() {
    ST::Vertex v0, v1, v2, v3;

    v0.position = ST::Vector3(-0.5f, -0.5f, 0);
    v0.normal = ST::Vector3(0, 0, 1);
    v0.texCoord = ST::Vector2(0, 0);
    v0.color = ST::Color(1, 0, 0, 1);

    v1.position = ST::Vector3(0.5f, -0.5f, 0);
    v1.normal = ST::Vector3(0, 0, 1);
    v1.texCoord = ST::Vector2(1, 0);
    v1.color = ST::Color(0, 1, 0, 1);

    v2.position = ST::Vector3(0.5f, 0.5f, 0);
    v2.normal = ST::Vector3(0, 0, 1);
    v2.texCoord = ST::Vector2(1, 1);
    v2.color = ST::Color(0, 0, 1, 1);

    v3.position = ST::Vector3(-0.5f, 0.5f, 0);
    v3.normal = ST::Vector3(0, 0, 1);
    v3.texCoord = ST::Vector2(0, 1);
    v3.color = ST::Color(1, 1, 0, 1);

    m_mesh.addVertex(v0);
    m_mesh.addVertex(v1);
    m_mesh.addVertex(v2);
    m_mesh.addVertex(v3);

    m_mesh.addIndex(0);
    m_mesh.addIndex(1);
    m_mesh.addIndex(2);

    m_mesh.addIndex(0);
    m_mesh.addIndex(2);
    m_mesh.addIndex(3);
}

void TestModule_VertexShader::setupUniforms(int canvasW, int canvasH) {
    float aspect = (float)canvasW / (float)canvasH;

    float sX = std::sin(m_rotation);
    float cX = std::cos(m_rotation);

    ST::Matrix4x4 model;
    model.m[0][0] = m_scaleX * cX;
    model.m[0][1] = -m_scaleY * sX;
    model.m[1][0] = m_scaleX * sX;
    model.m[1][1] = m_scaleY * cX;
    model.m[0][3] = m_posX * aspect;
    model.m[1][3] = m_posY;

    ST::Matrix4x4 projection;
    projection.m[0][0] = 1.0f / aspect;
    projection.m[1][1] = 1.0f;
    projection.m[2][2] = 1.0f;

    m_vertexShader.uniforms.modelMatrix = model;
    m_vertexShader.uniforms.viewMatrix = ST::Matrix4x4::identity();
    m_vertexShader.uniforms.projectionMatrix = projection;
}

void TestModule_VertexShader::render(void* canvasTexture, int canvasW, int canvasH) {
    if (!m_frameBuffer || m_frameBuffer->getWidth() != canvasW || m_frameBuffer->getHeight() != canvasH) {
        delete m_frameBuffer;
        delete m_depthBuffer;
        m_frameBuffer = new ST::FrameBuffer();
        m_frameBuffer->initialize(canvasW, canvasH);
        m_depthBuffer = new ST::DepthBuffer();
        m_depthBuffer->initialize(canvasW, canvasH);
    }

    m_frameBuffer->clear(ST::Color(0.2f, 0.2f, 0.2f, 1.0f));
    m_depthBuffer->clear();
    m_rasterizer.setBuffers(m_frameBuffer, m_depthBuffer);
    m_rasterizer.setUseRawScreenCoords(false);

    setupUniforms(canvasW, canvasH);

    const auto& vertices = m_mesh.getVertices();
    const auto& indices = m_mesh.getIndices();

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        int i0 = indices[i];
        int i1 = indices[i + 1];
        int i2 = indices[i + 2];

        const ST::Vertex& in0 = vertices[i0];
        const ST::Vertex& in1 = vertices[i1];
        const ST::Vertex& in2 = vertices[i2];

        // 通过 VertexShader 处理顶点
        ST::VertexOut v0 = m_vertexShader.process(in0);
        ST::VertexOut v1 = m_vertexShader.process(in1);
        ST::VertexOut v2 = m_vertexShader.process(in2);

        // ===== 2.2 光栅化器测试 =====
        ST::Vector2 s0 = m_rasterizer.toScreenSpace(v0);
        ST::Vector2 s1 = m_rasterizer.toScreenSpace(v1);
        ST::Vector2 s2 = m_rasterizer.toScreenSpace(v2);

        std::cout << "=== 2.2 光栅化器测试 ===" << std::endl;
        std::cout << "[NDC] v0=(" << v0.toNDC().x << "," << v0.toNDC().y << "," << v0.toNDC().z << ")" << std::endl;
        std::cout << "[NDC] v1=(" << v1.toNDC().x << "," << v1.toNDC().y << "," << v1.toNDC().z << ")" << std::endl;
        std::cout << "[NDC] v2=(" << v2.toNDC().x << "," << v2.toNDC().y << "," << v2.toNDC().z << ")" << std::endl;
        std::cout << "[Screen] s0=(" << s0.x << "," << s0.y << ")" << std::endl;
        std::cout << "[Screen] s1=(" << s1.x << "," << s1.y << ")" << std::endl;
        std::cout << "[Screen] s2=(" << s2.x << "," << s2.y << ")" << std::endl;

        float minX_val = std::min(std::min(s0.x, s1.x), s2.x);
        float maxX_val = std::max(std::max(s0.x, s1.x), s2.x);
        float minY_val = std::min(std::min(s0.y, s1.y), s2.y);
        float maxY_val = std::max(std::max(s0.y, s1.y), s2.y);

        int minX = static_cast<int>(std::max(0.0f, minX_val));
        int maxX = static_cast<int>(std::min((float)canvasW - 1, maxX_val));
        int minY = static_cast<int>(std::max(0.0f, minY_val));
        int maxY = static_cast<int>(std::min((float)canvasH - 1, maxY_val));

        // 保存第一个三角形的包围盒用于调试绘制
        if (i == 0) {
            m_debugBboxMinX = minX;
            m_debugBboxMaxX = maxX;
            m_debugBboxMinY = minY;
            m_debugBboxMaxY = maxY;
        }

        std::cout << "[BBox] minX=" << minX << " maxX=" << maxX << " minY=" << minY << " maxY=" << maxY << std::endl;
        std::cout << "[ScreenSize] width=" << canvasW << " height=" << canvasH << std::endl;
        // ===== 测试结束 =====

        m_rasterizer.rasterizeTriangle(v0, v1, v2,
            [](const ST::VertexOut& frag) {
                return frag.color;
            });
    }

    auto& pixels = m_frameBuffer->getPixels();
    SDL_Renderer* renderer = (SDL_Renderer*)canvasTexture;

    if (m_renderMode == 0) {
        for (int y = 0; y < canvasH; y++) {
            for (int x = 0; x < canvasW; x++) {
                const auto& col = pixels[y * canvasW + x];
                SDL_SetRenderDrawColor(renderer, (Uint8)(col.r * 255), (Uint8)(col.g * 255), (Uint8)(col.b * 255), 255);
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
        // 绘制红色 AABB 包围盒边框
        if (m_showDebugBbox) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMinY, m_debugBboxMaxX, m_debugBboxMinY);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMaxY, m_debugBboxMaxX, m_debugBboxMaxY);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMinY, m_debugBboxMinX, m_debugBboxMaxY);
            SDL_RenderDrawLine(renderer, m_debugBboxMaxX, m_debugBboxMinY, m_debugBboxMaxX, m_debugBboxMaxY);
        }
    } else {
        std::vector<uint32_t> sdlPixels(canvasW * canvasH);
        for (int i = 0; i < canvasW * canvasH; i++) {
            uint8_t r = (uint8_t)(pixels[i].r * 255);
            uint8_t g = (uint8_t)(pixels[i].g * 255);
            uint8_t b = (uint8_t)(pixels[i].b * 255);
            sdlPixels[i] = (r) | (g << 8) | (b << 16) | (0xFF << 24);
        }
        SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
            (void*)sdlPixels.data(), canvasW, canvasH, 32, canvasW * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_DestroyTexture(texture);

        // 绘制红色 AABB 包围盒边框
        if (m_showDebugBbox) {
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMinY, m_debugBboxMaxX, m_debugBboxMinY);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMaxY, m_debugBboxMaxX, m_debugBboxMaxY);
            SDL_RenderDrawLine(renderer, m_debugBboxMinX, m_debugBboxMinY, m_debugBboxMinX, m_debugBboxMaxY);
            SDL_RenderDrawLine(renderer, m_debugBboxMaxX, m_debugBboxMinY, m_debugBboxMaxX, m_debugBboxMaxY);
        }
    }
}

bool TestModule_VertexShader::renderControls() {
    ImGui::Separator();
    ImGui::Text("VertexShader Controls");
    ImGui::Separator();

    float oldPX = m_posX, oldPY = m_posY;
    float oldR = m_rotation;
    float oldSX = m_scaleX, oldSY = m_scaleY;
    int oldMode = m_renderMode;

    ImGui::Text("Position:");
    ImGui::SliderFloat("Pos X", &m_posX, -1.0f, 1.0f);
    ImGui::SliderFloat("Pos Y", &m_posY, -1.0f, 1.0f);

    ImGui::Text("Rotation:");
    ImGui::SliderFloat("Rot Z", &m_rotation, -3.14159f, 3.14159f);
    ImGui::Text("(%.1f deg)", m_rotation * 180.0f / 3.14159f);

    ImGui::Text("Scale:");
    ImGui::SliderFloat("Scale X", &m_scaleX, 0.1f, 3.0f);
    ImGui::SliderFloat("Scale Y", &m_scaleY, 0.1f, 3.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Render Mode:");
    ImGui::RadioButton("DrawPoint", &m_renderMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Texture", &m_renderMode, 1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Debug:");
    bool oldShowBbox = m_showDebugBbox;
    ImGui::Checkbox("Show AABB BBox", &m_showDebugBbox);

    bool changed = false;

    ImGui::Text("Position:");
    ImGui::SliderFloat("Pos X", &m_posX, -1.0f, 1.0f);
    ImGui::SliderFloat("Pos Y", &m_posY, -1.0f, 1.0f);

    ImGui::Text("Rotation:");
    ImGui::SliderFloat("Rot Z", &m_rotation, -3.14159f, 3.14159f);
    ImGui::Text("(%.1f deg)", m_rotation * 180.0f / 3.14159f);

    ImGui::Text("Scale:");
    ImGui::SliderFloat("Scale X", &m_scaleX, 0.1f, 3.0f);
    ImGui::SliderFloat("Scale Y", &m_scaleY, 0.1f, 3.0f);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Render Mode:");
    ImGui::RadioButton("DrawPoint", &m_renderMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Texture", &m_renderMode, 1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Debug:");
    ImGui::Checkbox("Show AABB BBox", &m_showDebugBbox);

    if (oldPX != m_posX || oldPY != m_posY || oldR != m_rotation ||
        oldSX != m_scaleX || oldSY != m_scaleY || oldMode != m_renderMode || oldShowBbox != m_showDebugBbox) {
        changed = true;
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset")) {
        m_posX = 0.0f;
        m_posY = 0.0f;
        m_rotation = 0.0f;
        m_scaleX = 1.0f;
        m_scaleY = 1.0f;
        changed = true;
    }

    return changed;
}

void TestModule_VertexShader::runConsole(std::string& output) {
    output = "[VertexShader Test] 2.1\n";
    output += "Position: (" + std::to_string(m_posX) + ", " + std::to_string(m_posY) + ")\n";
    output += "Rotation: " + std::to_string(m_rotation * 180.0f / 3.14159f) + " deg\n";
    output += "Scale: (" + std::to_string(m_scaleX) + ", " + std::to_string(m_scaleY) + ")\n";
}
