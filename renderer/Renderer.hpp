#pragma once
#include "FrameBuffer.hpp"
#include "DepthBuffer.hpp"
#include "VertexShader.hpp"
#include "FragmentShader.hpp"
#include "Rasterizer.hpp"
#include "Transform.hpp"
#include "Camera.hpp"

namespace ST {
    class Mesh;

    class Renderer {
    public:
        Renderer(int width, int height);
        ~Renderer();

        void setCamera(const Camera& camera);
        void render(const Mesh& mesh);

        void clear(const Color& color = Color::black());
        void clearDepth();

        int getWidth() const;
        int getHeight() const;
        const FrameBuffer& getFrameBuffer() const;
        const DepthBuffer& getDepthBuffer() const;

        void addLight(const Light& light) { m_fragmentShader.addLight(light); }
        void setModelMatrix(const Matrix4x4& mat);
        void setViewMatrix(const Matrix4x4& mat);

    private:
        void setupUniforms();

        int m_width;
        int m_height;

        FrameBuffer m_frameBuffer;
        DepthBuffer m_depthBuffer;

        const Camera* m_camera;
        VertexShader m_vertexShader;
        FragmentShader m_fragmentShader;
        Rasterizer m_rasterizer;
    };
}
