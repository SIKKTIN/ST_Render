#pragma once
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/FragmentShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/shader/ShaderProgram.hpp"
#include "renderer/shader/ShaderManager.hpp"
#include "renderer/transform/Transform.hpp"
#include "core/camera/Camera.hpp"
#include <memory>
#include <string>

namespace ST {
    class Mesh;

    class Renderer {
    public:
        Renderer(int width, int height);
        ~Renderer();

        void setCamera(const Camera& camera);
        void render(const Mesh& mesh);

        // Replace both programmable stages. Passing nullptr restores the
        // built-in transform and Blinn-Phong implementation.
        void setShaderProgram(std::shared_ptr<IShaderProgram> program);
        void resetShaderProgram();
        const std::shared_ptr<IShaderProgram>& getShaderProgram() const { return m_shaderProgram; }
        bool loadShaderFile(const std::string& path, std::string& error);
        bool reloadShaderIfChanged(std::string& error);
        const std::string& getShaderError() const { return m_shaderManager.getLastError(); }

        void clear(const Color& color = Color::black());
        void clearDepth();

        int getWidth() const;
        int getHeight() const;
        const FrameBuffer& getFrameBuffer() const;
        const DepthBuffer& getDepthBuffer() const;

        void addLight(const Light& light) { m_fragmentShader.addLight(light); }
        void setTexture(const std::vector<Color>& texture, int width, int height) {
            m_fragmentShader.setTexture(texture, width, height);
        }
        void setShaderFloat(const std::string& name, float value) { m_shaderParameters.floats[name] = value; }
        void setShaderVector2(const std::string& name, const Vector2& value) { m_shaderParameters.vec2s[name] = value; }
        void setShaderVector3(const std::string& name, const Vector3& value) { m_shaderParameters.vec3s[name] = value; }
        void setShaderVector4(const std::string& name, const Vector4& value) { m_shaderParameters.vec4s[name] = value; }
        void setShaderMatrix(const std::string& name, const Matrix4x4& value) { m_shaderParameters.matrices[name] = value; }
        void clearShaderParameters() { m_shaderParameters.clear(); }
        void setModelMatrix(const Matrix4x4& mat);
        void setViewMatrix(const Matrix4x4& mat);
        void setBackFaceCulling(bool enabled) { m_backFaceCulling = enabled; }
        bool isBackFaceCullingEnabled() const { return m_backFaceCulling; }

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
        bool m_backFaceCulling;
        std::shared_ptr<IShaderProgram> m_shaderProgram;
        ShaderManager m_shaderManager;
        ShaderParameters m_shaderParameters;
    };
}
