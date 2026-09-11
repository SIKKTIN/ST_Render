#include "renderer/renderer/Renderer.hpp"
#include "core/camera/Camera.hpp"
#include "renderer/geometry/Mesh.hpp"
#include <cmath>

namespace ST {
    Renderer::Renderer(int width, int height)
        : m_width(width)
        , m_height(height)
        , m_frameBuffer()
        , m_depthBuffer()
        , m_camera(nullptr)
        , m_vertexShader()
        , m_fragmentShader()
        , m_rasterizer()
        , m_backFaceCulling(true) {
        m_frameBuffer.initialize(width, height);
        m_depthBuffer.initialize(width, height);
        m_rasterizer.setBuffers(&m_frameBuffer, &m_depthBuffer);
        setupUniforms();
    }

    Renderer::~Renderer() = default;

    void Renderer::setCamera(const Camera& camera) {
        m_camera = &camera;
        m_fragmentShader.setViewPosition(camera.m_eye);
        setupUniforms();
    }

    void Renderer::render(const Mesh& mesh) {
        const auto& vertices = mesh.getVertices();
        const auto& indices = mesh.getIndices();

        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            int i0 = indices[i];
            int i1 = indices[i + 1];
            int i2 = indices[i + 2];

            VertexOut v0 = m_vertexShader.process(vertices[i0]);
            VertexOut v1 = m_vertexShader.process(vertices[i1]);
            VertexOut v2 = m_vertexShader.process(vertices[i2]);

            VertexOut clipped[16];
            int clippedCount = 0;
            clipTriangleAgainstFrustum(v0, v1, v2, clipped, clippedCount);
            if (clippedCount < 3) continue;

            // The clipped polygon is convex, so a triangle fan is sufficient.
            for (int k = 1; k + 1 < clippedCount; ++k) {
                const VertexOut& a = clipped[0];
                const VertexOut& b = clipped[k];
                const VertexOut& c = clipped[k + 1];

                if (m_backFaceCulling) {
                    Vector3 na = a.toNDC();
                    Vector3 nb = b.toNDC();
                    Vector3 nc = c.toNDC();
                    float area = (nb.x - na.x) * (nc.y - na.y)
                               - (nb.y - na.y) * (nc.x - na.x);
                    // NDC is y-up; front faces use the conventional CCW
                    // winding and therefore have positive signed area.
                    if (!std::isfinite(area) || area <= EPSILON) continue;
                }

                m_rasterizer.rasterizeTriangle(a, b, c,
                    [this](const VertexOut& vo) {
                        return m_fragmentShader.shade(vo);
                    });
            }
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
        m_vertexShader.uniforms.normalMatrix = mat.inverse().transpose();
    }

    void Renderer::setViewMatrix(const Matrix4x4& mat) {
        m_vertexShader.uniforms.viewMatrix = mat;
    }

    void Renderer::setupUniforms() {
        Uniform uni;
        uni.modelMatrix = Matrix4x4::identity();
        uni.normalMatrix = uni.modelMatrix.inverse().transpose();
        uni.viewMatrix = m_camera ? m_camera->getViewMatrix() : Matrix4x4::identity();
        uni.projectionMatrix = m_camera ? m_camera->getProjectionMatrix() : Matrix4x4::identity();
        uni.viewportMatrix = getViewportMatrix(m_width, m_height);

        m_vertexShader.setUniform(uni);
    }
}
