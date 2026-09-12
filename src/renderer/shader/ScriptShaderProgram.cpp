#include "renderer/shader/ScriptShaderProgram.hpp"

#include "core/math/MathUtils.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace ST {
namespace {

enum class ValueKind { Invalid, Float, Vec2, Vec3, Vec4, Matrix4 };

struct ShaderValue {
    ValueKind kind = ValueKind::Invalid;
    float scalar = 0.0f;
    Vector2 vec2;
    Vector3 vec3;
    Vector4 vec4;
    Matrix4x4 matrix;

    static ShaderValue fromFloat(float value) {
        ShaderValue result; result.kind = ValueKind::Float; result.scalar = value; return result;
    }
    static ShaderValue fromVec2(const Vector2& value) {
        ShaderValue result; result.kind = ValueKind::Vec2; result.vec2 = value; return result;
    }
    static ShaderValue fromVec3(const Vector3& value) {
        ShaderValue result; result.kind = ValueKind::Vec3; result.vec3 = value; return result;
    }
    static ShaderValue fromVec4(const Vector4& value) {
        ShaderValue result; result.kind = ValueKind::Vec4; result.vec4 = value; return result;
    }
    static ShaderValue fromMatrix(const Matrix4x4& value) {
        ShaderValue result; result.kind = ValueKind::Matrix4; result.matrix = value; return result;
    }
};

enum class TokenKind {
    End, Identifier, Number, Plus, Minus, Star, Slash, Dot, Comma,
    Equal, LParen, RParen, LBrace, RBrace, Semicolon
};

struct Token {
    TokenKind kind = TokenKind::End;
    std::string text;
    float number = 0.0f;
    int line = 1;
    int column = 1;
};

class Lexer {
public:
    explicit Lexer(const std::string& source) : m_source(source) {}

    std::vector<Token> tokenize(std::string& error) {
        std::vector<Token> tokens;
        while (!atEnd()) {
            char c = peek();
            if (std::isspace(static_cast<unsigned char>(c))) {
                advanceWhitespace();
                continue;
            }
            if (c == '/' && peekNext() == '/') {
                skipComment();
                continue;
            }

            Token token;
            token.line = m_line;
            token.column = m_column;
            switch (c) {
            case '+': token.kind = TokenKind::Plus; advance(); break;
            case '-': token.kind = TokenKind::Minus; advance(); break;
            case '*': token.kind = TokenKind::Star; advance(); break;
            case '/': token.kind = TokenKind::Slash; advance(); break;
            case '.': token.kind = TokenKind::Dot; advance(); break;
            case ',': token.kind = TokenKind::Comma; advance(); break;
            case '=': token.kind = TokenKind::Equal; advance(); break;
            case '(': token.kind = TokenKind::LParen; advance(); break;
            case ')': token.kind = TokenKind::RParen; advance(); break;
            case '{': token.kind = TokenKind::LBrace; advance(); break;
            case '}': token.kind = TokenKind::RBrace; advance(); break;
            case ';': token.kind = TokenKind::Semicolon; advance(); break;
            default:
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    token = identifier();
                } else if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                    token = number();
                } else {
                    errorAt(token, "unexpected character");
                    return {};
                }
                break;
            }
            tokens.push_back(std::move(token));
        }
        tokens.push_back(Token{});
        return tokens;
    }

private:
    bool atEnd() const { return m_offset >= m_source.size(); }
    char peek() const { return atEnd() ? '\0' : m_source[m_offset]; }
    char peekNext() const { return m_offset + 1 >= m_source.size() ? '\0' : m_source[m_offset + 1]; }

    void advance() {
        if (atEnd()) return;
        if (m_source[m_offset++] == '\n') { ++m_line; m_column = 1; }
        else { ++m_column; }
    }

    void advanceWhitespace() { while (!atEnd() && std::isspace(static_cast<unsigned char>(peek()))) advance(); }

    void skipComment() {
        while (!atEnd() && peek() != '\n') advance();
    }

    Token identifier() {
        Token token; token.kind = TokenKind::Identifier; token.line = m_line; token.column = m_column;
        size_t start = m_offset;
        while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) advance();
        token.text = m_source.substr(start, m_offset - start);
        return token;
    }

    Token number() {
        Token token; token.kind = TokenKind::Number; token.line = m_line; token.column = m_column;
        size_t start = m_offset;
        bool exponent = false;
        while (!atEnd()) {
            char c = peek();
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') { advance(); continue; }
            if ((c == 'e' || c == 'E') && !exponent) {
                exponent = true; advance();
                if (peek() == '+' || peek() == '-') advance();
                continue;
            }
            break;
        }
        token.text = m_source.substr(start, m_offset - start);
        try { token.number = std::stof(token.text); }
        catch (...) { token.kind = TokenKind::End; }
        return token;
    }

    void errorAt(const Token& token, const char* message) {
        std::ostringstream out;
        out << "line " << token.line << ", column " << token.column << ": " << message;
        m_error = out.str();
    }

public:
    const std::string& error() const { return m_error; }

private:
    const std::string& m_source;
    size_t m_offset = 0;
    int m_line = 1;
    int m_column = 1;
    std::string m_error;
};

struct Expr {
    enum class Kind { Number, Identifier, Unary, Binary, Call } kind;
    float number = 0.0f;
    char op = 0;
    std::string name;
    std::shared_ptr<Expr> left;
    std::shared_ptr<Expr> right;
    std::vector<std::shared_ptr<Expr>> args;
};

using ExprPtr = std::shared_ptr<Expr>;

enum class TargetKind { Position, Normal, UV, WorldPosition, Color, Varying };

struct ScriptStatement {
    TargetKind target = TargetKind::Color;
    int varyingIndex = -1;
    ExprPtr expression;
};

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : m_tokens(tokens) {}

    bool parse(std::vector<ScriptStatement>& vertex,
               std::vector<ScriptStatement>& fragment,
               std::string& error) {
        while (!check(TokenKind::End)) {
            if (!checkIdentifier("vertex") && !checkIdentifier("fragment")) {
                return fail(peek(), "expected vertex or fragment block", error);
            }
            bool isVertex = checkIdentifier("vertex");
            advance();
            if (!match(TokenKind::LBrace)) return fail(peek(), "expected '{'", error);
            auto& output = isVertex ? vertex : fragment;
            while (!check(TokenKind::RBrace) && !check(TokenKind::End)) {
                ScriptStatement statement;
                if (!parseTarget(isVertex, statement, error)) return false;
                if (!match(TokenKind::Equal)) return fail(peek(), "expected '='", error);
                statement.expression = parseExpression(error);
                if (!statement.expression) return false;
                if (!match(TokenKind::Semicolon)) return fail(peek(), "expected ';'", error);
                output.push_back(std::move(statement));
            }
            if (!match(TokenKind::RBrace)) return fail(peek(), "expected '}'", error);
        }
        return true;
    }

private:
    bool check(TokenKind kind) const { return peek().kind == kind; }
    bool checkIdentifier(const char* value) const {
        return peek().kind == TokenKind::Identifier && peek().text == value;
    }
    const Token& peek() const { return m_tokens[m_index]; }
    const Token& previous() const { return m_tokens[m_index - 1]; }
    void advance() { if (!check(TokenKind::End)) ++m_index; }
    bool match(TokenKind kind) { if (!check(kind)) return false; advance(); return true; }

    bool fail(const Token& token, const char* message, std::string& error) const {
        std::ostringstream out;
        out << "line " << token.line << ", column " << token.column << ": " << message;
        error = out.str();
        return false;
    }

    bool parseTarget(bool isVertex, ScriptStatement& statement, std::string& error) {
        if (!check(TokenKind::Identifier)) return fail(peek(), "expected output field", error);
        std::string prefix = peek().text;
        advance();
        if (prefix == "output" || prefix == "vertex" || prefix == "fragment") {
            if (!match(TokenKind::Dot) || !check(TokenKind::Identifier)) {
                return fail(peek(), "expected output field after '.'", error);
            }
            prefix = peek().text;
            advance();
        }

        if (prefix == "position") statement.target = TargetKind::Position;
        else if (prefix == "normal") statement.target = TargetKind::Normal;
        else if (prefix == "uv" || prefix == "texCoord") statement.target = TargetKind::UV;
        else if (prefix == "worldPosition") statement.target = TargetKind::WorldPosition;
        else if (prefix == "color") statement.target = TargetKind::Color;
        else if (prefix.rfind("varying", 0) == 0 && prefix.size() > 7) {
            try { statement.varyingIndex = std::stoi(prefix.substr(7)); }
            catch (...) { statement.varyingIndex = -1; }
            if (statement.varyingIndex < 0 || statement.varyingIndex >= static_cast<int>(MAX_VARYINGS)) {
                return fail(previous(), "varying index is out of range", error);
            }
            statement.target = TargetKind::Varying;
        } else {
            return fail(previous(), "unknown output field", error);
        }

        if (!isVertex && statement.target != TargetKind::Color && statement.target != TargetKind::Varying) {
            return fail(previous(), "fragment stage can only write color or varying", error);
        }
        return true;
    }

    ExprPtr parseExpression(std::string& error) { return parseAddSub(error); }

    ExprPtr parseAddSub(std::string& error) {
        ExprPtr expression = parseMulDiv(error);
        while (expression && (check(TokenKind::Plus) || check(TokenKind::Minus))) {
            char op = check(TokenKind::Plus) ? '+' : '-'; advance();
            ExprPtr rhs = parseMulDiv(error); if (!rhs) return nullptr;
            auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Binary; node->op = op;
            node->left = expression; node->right = rhs; expression = node;
        }
        return expression;
    }

    ExprPtr parseMulDiv(std::string& error) {
        ExprPtr expression = parseUnary(error);
        while (expression && (check(TokenKind::Star) || check(TokenKind::Slash))) {
            char op = check(TokenKind::Star) ? '*' : '/'; advance();
            ExprPtr rhs = parseUnary(error); if (!rhs) return nullptr;
            auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Binary; node->op = op;
            node->left = expression; node->right = rhs; expression = node;
        }
        return expression;
    }

    ExprPtr parseUnary(std::string& error) {
        if (match(TokenKind::Minus)) {
            auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Unary; node->op = '-';
            node->left = parseUnary(error); return node->left ? node : nullptr;
        }
        return parsePrimary(error);
    }

    ExprPtr parsePrimary(std::string& error) {
        if (match(TokenKind::Number)) {
            auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Number; node->number = previous().number; return node;
        }
        if (match(TokenKind::LParen)) {
            ExprPtr expression = parseExpression(error);
            if (!match(TokenKind::RParen)) { fail(peek(), "expected ')'", error); return nullptr; }
            return expression;
        }
        if (!check(TokenKind::Identifier)) { fail(peek(), "expected expression", error); return nullptr; }

        std::string name = peek().text; advance();
        if (match(TokenKind::LParen)) {
            auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Call; node->name = name;
            if (!check(TokenKind::RParen)) {
                do {
                    ExprPtr argument = parseExpression(error); if (!argument) return nullptr;
                    node->args.push_back(std::move(argument));
                } while (match(TokenKind::Comma));
            }
            if (!match(TokenKind::RParen)) { fail(peek(), "expected ')'", error); return nullptr; }
            return node;
        }

        while (match(TokenKind::Dot)) {
            if (!check(TokenKind::Identifier)) { fail(peek(), "expected identifier after '.'", error); return nullptr; }
            name += "." + peek().text; advance();
        }
        auto node = std::make_shared<Expr>(); node->kind = Expr::Kind::Identifier; node->name = std::move(name); return node;
    }

    const std::vector<Token>& m_tokens;
    size_t m_index = 0;
};

struct EvalEnvironment {
    const VertexOut* vertexInput = nullptr;
    const Vertex* rawVertex = nullptr;
    const ShaderContext* context = nullptr;
};

bool sameKind(const ShaderValue& a, const ShaderValue& b) { return a.kind == b.kind; }

bool binaryValue(char op, const ShaderValue& a, const ShaderValue& b, ShaderValue& out) {
    if (a.kind == ValueKind::Matrix4 && b.kind == ValueKind::Vec4 && op == '*') {
        out = ShaderValue::fromVec4(a.matrix * b.vec4); return true;
    }
    if (a.kind == ValueKind::Matrix4 && b.kind == ValueKind::Matrix4 && op == '*') {
        out = ShaderValue::fromMatrix(a.matrix * b.matrix); return true;
    }
    if (op == '*' && a.kind == ValueKind::Float) {
        if (b.kind == ValueKind::Vec2) { out = ShaderValue::fromVec2(b.vec2 * a.scalar); return true; }
        if (b.kind == ValueKind::Vec3) { out = ShaderValue::fromVec3(b.vec3 * a.scalar); return true; }
        if (b.kind == ValueKind::Vec4) { out = ShaderValue::fromVec4(b.vec4 * a.scalar); return true; }
    }
    if (op == '*' && b.kind == ValueKind::Float) {
        if (a.kind == ValueKind::Vec2) { out = ShaderValue::fromVec2(a.vec2 * b.scalar); return true; }
        if (a.kind == ValueKind::Vec3) { out = ShaderValue::fromVec3(a.vec3 * b.scalar); return true; }
        if (a.kind == ValueKind::Vec4) { out = ShaderValue::fromVec4(a.vec4 * b.scalar); return true; }
    }
    if (sameKind(a, b)) {
        if (a.kind == ValueKind::Float) {
            out = ShaderValue::fromFloat(op == '+' ? a.scalar + b.scalar : op == '-' ? a.scalar - b.scalar : op == '*' ? a.scalar * b.scalar : a.scalar / b.scalar); return true;
        }
        if (a.kind == ValueKind::Vec2 && (op == '+' || op == '-')) { out = ShaderValue::fromVec2(op == '+' ? a.vec2 + b.vec2 : a.vec2 - b.vec2); return true; }
        if (a.kind == ValueKind::Vec3 && (op == '+' || op == '-')) { out = ShaderValue::fromVec3(op == '+' ? a.vec3 + b.vec3 : a.vec3 - b.vec3); return true; }
        if (a.kind == ValueKind::Vec4 && (op == '+' || op == '-')) { out = ShaderValue::fromVec4(op == '+' ? a.vec4 + b.vec4 : a.vec4 - b.vec4); return true; }
    }
    return false;
}

bool evalExpression(const ExprPtr& expression, const EvalEnvironment& env,
                    ShaderValue& result, std::string& error);

bool resolveIdentifier(const std::string& name, const EvalEnvironment& env,
                       ShaderValue& result) {
    if (!env.context) return false;
    if (name == "input.position") {
        if (env.rawVertex) result = ShaderValue::fromVec4(Vector4(env.rawVertex->position, 1.0f));
        else result = ShaderValue::fromVec4(env.vertexInput->position);
        return true;
    }
    if (name == "input.normal") {
        result = ShaderValue::fromVec3(env.rawVertex ? env.rawVertex->normal : env.vertexInput->normal);
        return true;
    }
    if (name == "input.uv" || name == "input.texCoord") {
        result = ShaderValue::fromVec2(env.rawVertex ? env.rawVertex->texCoord : env.vertexInput->texCoord);
        return true;
    }
    if (name == "input.color") {
        result = ShaderValue::fromVec4(env.rawVertex ? Vector4(env.rawVertex->color) : Vector4(env.vertexInput->color));
        return true;
    }
    if (name == "input.worldPosition") { result = ShaderValue::fromVec3(env.vertexInput->worldPosition); return true; }
    if (name == "uniform.model") { result = ShaderValue::fromMatrix(env.context->uniforms.modelMatrix); return true; }
    if (name == "uniform.view") { result = ShaderValue::fromMatrix(env.context->uniforms.viewMatrix); return true; }
    if (name == "uniform.projection") { result = ShaderValue::fromMatrix(env.context->uniforms.projectionMatrix); return true; }
    if (name == "uniform.mvp") { result = ShaderValue::fromMatrix(env.context->uniforms.getMvpMatrix()); return true; }
    if (name == "uniform.normal") { result = ShaderValue::fromMatrix(env.context->uniforms.normalMatrix); return true; }

    const std::string parameterName = name.rfind("uniform.", 0) == 0
        ? name.substr(8) : std::string();
    if (!parameterName.empty()) {
        const auto floatIt = env.context->parameters.floats.find(parameterName);
        if (floatIt != env.context->parameters.floats.end()) {
            result = ShaderValue::fromFloat(floatIt->second); return true;
        }
        const auto vec2It = env.context->parameters.vec2s.find(parameterName);
        if (vec2It != env.context->parameters.vec2s.end()) {
            result = ShaderValue::fromVec2(vec2It->second); return true;
        }
        const auto vec3It = env.context->parameters.vec3s.find(parameterName);
        if (vec3It != env.context->parameters.vec3s.end()) {
            result = ShaderValue::fromVec3(vec3It->second); return true;
        }
        const auto vec4It = env.context->parameters.vec4s.find(parameterName);
        if (vec4It != env.context->parameters.vec4s.end()) {
            result = ShaderValue::fromVec4(vec4It->second); return true;
        }
        const auto matrixIt = env.context->parameters.matrices.find(parameterName);
        if (matrixIt != env.context->parameters.matrices.end()) {
            result = ShaderValue::fromMatrix(matrixIt->second); return true;
        }
    }

    if (name.rfind("input.varying", 0) == 0) {
        int index = -1; try { index = std::stoi(name.substr(13)); } catch (...) { return false; }
        if (index >= 0 && index < static_cast<int>(MAX_VARYINGS)) {
            result = ShaderValue::fromVec4(env.vertexInput->varyings[index]); return true;
        }
    }
    return false;
}

bool evalCall(const Expr& expression, const EvalEnvironment& env,
              ShaderValue& result, std::string& error) {
    std::vector<ShaderValue> args;
    for (const auto& arg : expression.args) {
        ShaderValue value; if (!evalExpression(arg, env, value, error)) return false; args.push_back(value);
    }
    const std::string& name = expression.name;
    if (name == "vec2" || name == "vec3" || name == "vec4") {
        int expected = name == "vec2" ? 2 : name == "vec3" ? 3 : 4;
        std::vector<float> values;
        for (const auto& value : args) {
            if (value.kind == ValueKind::Float) values.push_back(value.scalar);
            else if (value.kind == ValueKind::Vec2) { values.push_back(value.vec2.x); values.push_back(value.vec2.y); }
            else if (value.kind == ValueKind::Vec3) { values.push_back(value.vec3.x); values.push_back(value.vec3.y); values.push_back(value.vec3.z); }
            else if (value.kind == ValueKind::Vec4) { values.push_back(value.vec4.x); values.push_back(value.vec4.y); values.push_back(value.vec4.z); values.push_back(value.vec4.w); }
        }
        if (static_cast<int>(values.size()) != expected) return false;
        if (expected == 2) result = ShaderValue::fromVec2(Vector2(values[0], values[1]));
        else if (expected == 3) result = ShaderValue::fromVec3(Vector3(values[0], values[1], values[2]));
        else result = ShaderValue::fromVec4(Vector4(values[0], values[1], values[2], values[3]));
        return true;
    }
    if (name == "normalize" && args.size() == 1) {
        if (args[0].kind == ValueKind::Vec3) { result = ShaderValue::fromVec3(args[0].vec3.normalized()); return true; }
        if (args[0].kind == ValueKind::Vec2) { result = ShaderValue::fromVec2(args[0].vec2.normalized()); return true; }
    }
    if (name == "dot" && args.size() == 2) {
        if (args[0].kind == ValueKind::Vec3 && args[1].kind == ValueKind::Vec3) { result = ShaderValue::fromFloat(args[0].vec3.dot(args[1].vec3)); return true; }
        if (args[0].kind == ValueKind::Vec2 && args[1].kind == ValueKind::Vec2) { result = ShaderValue::fromFloat(args[0].vec2.dot(args[1].vec2)); return true; }
    }
    if (name == "texture" && args.size() == 1 && args[0].kind == ValueKind::Vec2 &&
        env.context && env.context->sampleTexture) {
        result = ShaderValue::fromVec4(env.context->sampleTexture(args[0].vec2));
        return true;
    }
    if ((name == "clamp" || name == "saturate") && !args.empty()) {
        float lo = name == "saturate" ? 0.0f : args.size() > 1 && args[1].kind == ValueKind::Float ? args[1].scalar : 0.0f;
        float hi = name == "saturate" ? 1.0f : args.size() > 2 && args[2].kind == ValueKind::Float ? args[2].scalar : 1.0f;
        if (args[0].kind == ValueKind::Float) { result = ShaderValue::fromFloat(ST::clamp(args[0].scalar, lo, hi)); return true; }
    }
    if (name == "mix" && args.size() == 3 && args[2].kind == ValueKind::Float) {
        ShaderValue delta;
        if (binaryValue('-', args[1], args[0], delta)) {
            ShaderValue scaled;
            if (binaryValue('*', delta, args[2], scaled)) { binaryValue('+', args[0], scaled, result); return result.kind != ValueKind::Invalid; }
        }
    }
    return false;
}

bool evalExpression(const ExprPtr& expression, const EvalEnvironment& env,
                    ShaderValue& result, std::string& error) {
    if (!expression) return false;
    switch (expression->kind) {
    case Expr::Kind::Number: result = ShaderValue::fromFloat(expression->number); return true;
    case Expr::Kind::Identifier:
        if (resolveIdentifier(expression->name, env, result)) return true;
        error = "unknown identifier: " + expression->name; return false;
    case Expr::Kind::Unary: {
        if (!evalExpression(expression->left, env, result, error)) return false;
        if (expression->op == '-' && result.kind == ValueKind::Float) result.scalar = -result.scalar;
        else if (expression->op == '-' && result.kind == ValueKind::Vec2) result.vec2 = -1.0f * result.vec2;
        else if (expression->op == '-' && result.kind == ValueKind::Vec3) result.vec3 = -result.vec3;
        else if (expression->op == '-' && result.kind == ValueKind::Vec4) result.vec4 = result.vec4 * -1.0f;
        else { error = "unsupported unary expression"; return false; }
        return true;
    }
    case Expr::Kind::Binary: {
        ShaderValue left, right;
        if (!evalExpression(expression->left, env, left, error) || !evalExpression(expression->right, env, right, error)) return false;
        if (!binaryValue(expression->op, left, right, result)) { error = "incompatible operands"; return false; }
        return true;
    }
    case Expr::Kind::Call:
        if (evalCall(*expression, env, result, error)) return true;
        error = "invalid function call: " + expression->name; return false;
    }
    return false;
}

bool applyStatement(const ScriptStatement& statement, const EvalEnvironment& env,
                    VertexOut& output, std::string& error) {
    ShaderValue value;
    if (!evalExpression(statement.expression, env, value, error)) return false;
    switch (statement.target) {
    case TargetKind::Position:
        if (value.kind != ValueKind::Vec4) { error = "position expects vec4"; return false; }
        output.position = value.vec4; return true;
    case TargetKind::Normal:
        if (value.kind != ValueKind::Vec3) { error = "normal expects vec3"; return false; }
        output.normal = value.vec3; return true;
    case TargetKind::UV:
        if (value.kind != ValueKind::Vec2) { error = "uv expects vec2"; return false; }
        output.texCoord = value.vec2; return true;
    case TargetKind::WorldPosition:
        if (value.kind != ValueKind::Vec3) { error = "worldPosition expects vec3"; return false; }
        output.worldPosition = value.vec3; return true;
    case TargetKind::Varying:
        if (value.kind != ValueKind::Vec4) { error = "varying expects vec4"; return false; }
        output.setVarying(static_cast<size_t>(statement.varyingIndex), value.vec4); return true;
    case TargetKind::Color:
        if (value.kind != ValueKind::Vec4) { error = "color expects vec4"; return false; }
        output.color = Color(value.vec4); return true;
    }
    return false;
}

} // namespace

struct ScriptShaderProgram::Statement : ScriptStatement {};

ScriptShaderProgram::ScriptShaderProgram(const std::string& source) {
    std::string ignored;
    compile(source, ignored);
}

bool ScriptShaderProgram::compile(const std::string& source, std::string& error) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize(error);
    if (tokens.empty()) { if (error.empty()) error = lexer.error(); return false; }
    Parser parser(tokens);
    std::vector<ScriptStatement> vertex;
    std::vector<ScriptStatement> fragment;
    if (!parser.parse(vertex, fragment, error)) return false;
    m_vertexStatements.clear();
    m_fragmentStatements.clear();
    for (auto& statement : vertex) m_vertexStatements.push_back(Statement{std::move(statement)});
    for (auto& statement : fragment) m_fragmentStatements.push_back(Statement{std::move(statement)});

    // Common unlit scripts should not pay the full AST/value-construction cost
    // for every covered pixel. Keep this small fast path deliberately strict;
    // all other scripts continue through the general interpreter.
    m_fragmentFastPath = 0;
    m_fragmentFastPathVarying = -1;
    if (m_fragmentStatements.size() == 1 &&
        m_fragmentStatements[0].target == TargetKind::Color) {
        const ExprPtr& expression = m_fragmentStatements[0].expression;
        if (expression && expression->kind == Expr::Kind::Identifier) {
            if (expression->name == "input.color") {
                m_fragmentFastPath = 1;
            } else if (expression->name.rfind("input.varying", 0) == 0) {
                try {
                    m_fragmentFastPathVarying = std::stoi(expression->name.substr(13));
                    if (m_fragmentFastPathVarying >= 0 && m_fragmentFastPathVarying < static_cast<int>(MAX_VARYINGS)) {
                        m_fragmentFastPath = 2;
                    }
                } catch (...) {
                    m_fragmentFastPathVarying = -1;
                }
            }
        } else if (expression && expression->kind == Expr::Kind::Call &&
                   expression->name == "texture" && expression->args.size() == 1 &&
                   expression->args[0]->kind == Expr::Kind::Identifier &&
                   expression->args[0]->name == "input.uv") {
            m_fragmentFastPath = 3;
        }
    }
    m_source = source;
    return true;
}

bool ScriptShaderProgram::loadFile(const std::string& path, std::string& error) {
    std::ifstream file(path);
    if (!file) { error = "cannot open shader file: " + path; return false; }
    std::ostringstream source; source << file.rdbuf();
    return compile(source.str(), error);
}

std::shared_ptr<ScriptShaderProgram> ScriptShaderProgram::fromSource(
    const std::string& source, std::string& error) {
    auto program = std::make_shared<ScriptShaderProgram>();
    if (!program->compile(source, error)) return nullptr;
    return program;
}

std::shared_ptr<ScriptShaderProgram> ScriptShaderProgram::fromFile(
    const std::string& path, std::string& error) {
    auto program = std::make_shared<ScriptShaderProgram>();
    if (!program->loadFile(path, error)) return nullptr;
    return program;
}

VertexOut ScriptShaderProgram::vertex(const Vertex& input,
                                      const ShaderContext& context) const {
    VertexOut output = VertexShader::transformVertex(input, context.uniforms);
    EvalEnvironment environment{ &output, &input, &context };
    std::string error;
    for (const auto& statement : m_vertexStatements) {
        if (!applyStatement(statement, environment, output, error)) break;
    }
    return output;
}

Color ScriptShaderProgram::fragment(const VertexOut& input,
                                    const ShaderContext& context) const {
    if (m_fragmentFastPath == 1) return input.color;
    if (m_fragmentFastPath == 2) return Color(input.varyings[m_fragmentFastPathVarying]);
    if (m_fragmentFastPath == 3 && context.sampleTexture) return context.sampleTexture(input.texCoord);

    VertexOut output = input;
    EvalEnvironment environment{ &input, nullptr, &context };
    std::string error;
    for (const auto& statement : m_fragmentStatements) {
        if (!applyStatement(statement, environment, output, error)) break;
    }
    return output.color;
}

} // namespace ST
