#pragma once

#include "renderer/geometry/Vertex.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/FragmentShader.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>

namespace ST {

struct ShaderParameters {
    std::unordered_map<std::string, float> floats;
    std::unordered_map<std::string, Vector2> vec2s;
    std::unordered_map<std::string, Vector3> vec3s;
    std::unordered_map<std::string, Vector4> vec4s;
    std::unordered_map<std::string, Matrix4x4> matrices;

    void clear() {
        floats.clear(); vec2s.clear(); vec3s.clear(); vec4s.clear(); matrices.clear();
    }
};

// Per-draw data visible to both programmable stages.
struct ShaderContext {
    Uniform uniforms;
    Vector3 viewPosition;
    ShaderParameters parameters;
    std::function<Color(const Vector2&)> sampleTexture;
};

class IShaderProgram {
public:
    virtual ~IShaderProgram() = default;

    virtual VertexOut vertex(const Vertex& input,
                             const ShaderContext& context) const = 0;

    virtual Color fragment(const VertexOut& input,
                           const ShaderContext& context) const = 0;
};

// A native callback-backed program is the first programmable implementation.
// It lets tests, editor tools, and future script backends share exactly the
// same Renderer entry point without putting a scripting runtime in the hot
// rasterization loop yet.
class NativeShaderProgram final : public IShaderProgram {
public:
    using VertexFunction = std::function<VertexOut(const Vertex&, const ShaderContext&)>;
    using FragmentFunction = std::function<Color(const VertexOut&, const ShaderContext&)>;

    NativeShaderProgram() = default;

    NativeShaderProgram(VertexFunction vertexFunction,
                        FragmentFunction fragmentFunction)
        : m_vertexFunction(std::move(vertexFunction))
        , m_fragmentFunction(std::move(fragmentFunction)) {}

    void setVertexFunction(VertexFunction function) {
        m_vertexFunction = std::move(function);
    }

    void setFragmentFunction(FragmentFunction function) {
        m_fragmentFunction = std::move(function);
    }

    VertexOut vertex(const Vertex& input,
                     const ShaderContext& context) const override {
        return m_vertexFunction ? m_vertexFunction(input, context) : VertexOut();
    }

    Color fragment(const VertexOut& input,
                   const ShaderContext& context) const override {
        return m_fragmentFunction ? m_fragmentFunction(input, context) : Color::black();
    }

private:
    VertexFunction m_vertexFunction;
    FragmentFunction m_fragmentFunction;
};

// Compatibility implementation containing the renderer's current built-in
// transform and Blinn-Phong paths.
class BuiltinShaderProgram final : public IShaderProgram {
public:
    BuiltinShaderProgram(VertexShader& vertexShader,
                         FragmentShader& fragmentShader)
        : m_vertexShader(&vertexShader)
        , m_fragmentShader(&fragmentShader) {}

    VertexOut vertex(const Vertex& input,
                     const ShaderContext&) const override {
        return m_vertexShader->process(input);
    }

    Color fragment(const VertexOut& input,
                   const ShaderContext&) const override {
        return m_fragmentShader->shade(input);
    }

private:
    VertexShader* m_vertexShader;
    FragmentShader* m_fragmentShader;
};

} // namespace ST
