#pragma once

#include "renderer/shader/ShaderProgram.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ST {

// A small, dependency-free shader language.  The compiler is deliberately
// separate from Renderer so a future bytecode VM or external language can
// implement the same IShaderProgram interface.
class ScriptShaderProgram final : public IShaderProgram {
public:
    ScriptShaderProgram() = default;
    explicit ScriptShaderProgram(const std::string& source);

    bool compile(const std::string& source, std::string& error);
    bool loadFile(const std::string& path, std::string& error);

    static std::shared_ptr<ScriptShaderProgram> fromSource(
        const std::string& source, std::string& error);
    static std::shared_ptr<ScriptShaderProgram> fromFile(
        const std::string& path, std::string& error);

    VertexOut vertex(const Vertex& input,
                     const ShaderContext& context) const override;

    Color fragment(const VertexOut& input,
                   const ShaderContext& context) const override;

    const std::string& getSource() const { return m_source; }

private:
    struct Statement;
    std::vector<Statement> m_vertexStatements;
    std::vector<Statement> m_fragmentStatements;
    std::string m_source;
    int m_fragmentFastPath = 0;
    int m_fragmentFastPathVarying = -1;
};

} // namespace ST
