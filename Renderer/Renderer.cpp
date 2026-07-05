#include "Renderer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"
#include <iostream>

namespace ST {
    Renderer::Renderer(int width, int height)
        : m_width(width)
        , m_height(height)
        , m_frameBuffer()
        , m_depthBuffer()
        , m_camera(nullptr)
        , m_vertexShader()
        , m_fragmentShader()
        , m_rasterizer() {
        m_frameBuffer.initialize(width, height);
        m_depthBuffer.initialize(width, height);
        m_rasterizer.setBuffers(&m_frameBuffer, &m_depthBuffer);
    }

    Renderer::~Renderer() = default;

    void Renderer::setCamera(const Camera& camera) {
        m_camera = &camera;
        setupUniforms();
    }

    void Renderer::render(const Mesh& mesh) {
        const auto& vertices = mesh.getVertices();
        const auto& indices = mesh.getIndices();

        static int callCount = 0;
        callCount++;
        if (callCount == 1) {
            std::cout << "[Renderer] Vertices: " << vertices.size() << ", Indices: " << indices.size() << std::endl;
            for (size_t i = 0; i + 2 < indices.size(); i += 3) {
                int i0 = indices[i];
                int i1 = indices[i + 1];
                int i2 = indices[i + 2];
                std::cout << "[Renderer] Triangle " << (i/3) << ": idx=" << i0 << "," << i1 << "," << i2 << std::endl;
                std::cout << "[Renderer]   v0=(" << vertices[i0].position.x << "," << vertices[i0].position.y << "," << vertices[i0].position.z << ")" << std::endl;
                std::cout << "[Renderer]   v1=(" << vertices[i1].position.x << "," << vertices[i1].position.y << "," << vertices[i1].position.z << ")" << std::endl;
                std::cout << "[Renderer]   v2=(" << vertices[i2].position.x << "," << vertices[i2].position.y << "," << vertices[i2].position.z << ")" << std::endl;
            }
        }

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int i0 = indices[i];
            int i1 = indices[i + 1];
            int i2 = indices[i + 2];

            VertexOut v0 = m_vertexShader.process(vertices[i0]);
            VertexOut v1 = m_vertexShader.process(vertices[i1]);
            VertexOut v2 = m_vertexShader.process(vertices[i2]);

            if (i == 0) {
                std::cout << "[Renderer] MVP*pos v0: (" << v0.position.x << "," << v0.position.y << "," << v0.position.z << "," << v0.position.w << ")" << std::endl;
                std::cout << "[Renderer] Color v0: (" << v0.color.r << "," << v0.color.g << "," << v0.color.b << ")" << std::endl;
                std::cout << "[Renderer] Width: " << m_width << " Height: " << m_height << std::endl;
            }

            m_rasterizer.rasterizeTriangle(v0, v1, v2,
                [this](const VertexOut& vo) {
                    return m_fragmentShader.shade(vo);
                });
        }
    }

    void Renderer::clear(const Color& color) {
        m_frameBuffer.clear(color);
        m_depthBuffer.clear();
    }

    void Renderer::clearDepth() {
        m_depthBuffer.clear();
    }

    int Renderer::getWidth() const {
        return m_width;
    }

    int Renderer::getHeight() const {
        return m_height;
    }

    const FrameBuffer& Renderer::getFrameBuffer() const {
        return m_frameBuffer;
    }

    const DepthBuffer& Renderer::getDepthBuffer() const {
        return m_depthBuffer;
    }

    void Renderer::setModelMatrix(const Matrix4x4& mat) {
        m_vertexShader.uniforms.modelMatrix = mat;
    }

    void Renderer::setViewMatrix(const Matrix4x4& mat) {
        m_vertexShader.uniforms.viewMatrix = mat;
    }

    void Renderer::setupUniforms() {
        Uniform uni;
        uni.modelMatrix = Matrix4x4::identity();
        uni.viewMatrix = m_camera ? m_camera->getViewMatrix() : Matrix4x4::identity();
        uni.projectionMatrix = m_camera ? m_camera->getProjectionMatrix() : Matrix4x4::identity();
        uni.viewportMatrix = getViewportMatrix(m_width, m_height);

        m_vertexShader.setUniform(uni);
    }
}
