#pragma once
#define NOMINMAX

#include "core/math/Vector3.hpp"
#include "core/math/Vector2.hpp"
#include "core/math/MathUtils.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include <functional>
#include <algorithm>
#include <cmath>

namespace ST {
class Rasterizer {
public:
    FrameBuffer* m_frameBuffer;
    DepthBuffer* m_depthBuffer;
    int m_width;
    int m_height;
    bool m_useRawScreenCoords;
    int m_viewportX;
    int m_viewportY;
    int m_viewportWidth;
    int m_viewportHeight;

    Rasterizer()
        : m_frameBuffer(nullptr)
        , m_depthBuffer(nullptr)
        , m_width(0)
        , m_height(0)
        , m_useRawScreenCoords(false)
        , m_viewportX(0)
        , m_viewportY(0)
        , m_viewportWidth(0)
        , m_viewportHeight(0) {}

    void setBuffers(FrameBuffer* fb, DepthBuffer* db) {
        m_frameBuffer = fb;
        m_depthBuffer = db;
        if (fb) {
            m_width = fb->getWidth();
            m_height = fb->getHeight();
            m_viewportX = 0;
            m_viewportY = 0;
            m_viewportWidth = m_width;
            m_viewportHeight = m_height;
        }
    }

    void setViewport(int x, int y, int width, int height) {
        m_viewportX = std::max(0, x);
        m_viewportY = std::max(0, y);
        m_viewportWidth = std::max(1, std::min(width, m_width - m_viewportX));
        m_viewportHeight = std::max(1, std::min(height, m_height - m_viewportY));
    }

    void setUseRawScreenCoords(bool useRaw) {
        m_useRawScreenCoords = useRaw;
    }

    Vector2 toScreenSpace(const VertexOut& v) {
        if (m_useRawScreenCoords) {
            return Vector2(v.position.x, v.position.y);
        }
        Vector3 ndc = v.toNDC();
        return Vector2(
            m_viewportX + (ndc.x + 1.0f) * 0.5f * m_viewportWidth,
            m_viewportY + (1.0f - ndc.y) * 0.5f * m_viewportHeight
        );
    }

    static Vector3 computeBarycentric(const Vector2& p,
        const Vector2& a, const Vector2& b, const Vector2& c) {
        float alpha = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) /
            ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));
        float beta = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) /
            ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));
        float gamma = 1.0f - alpha - beta;
        return Vector3(alpha, beta, gamma);
    }

    template<typename T>
    static T linearInterpolate(const Vector3& bary,
        const T& attr0, const T& attr1, const T& attr2) {
        return attr0 * bary.x + attr1 * bary.y + attr2 * bary.z;
    }

    template<typename T>
    static T perspectiveCorrect(const Vector3& bary,
        const T& attr0, const T& attr1, const T& attr2,
        float w0, float w1, float w2) {
        float oneOverW0 = 1.0f / w0;
        float oneOverW1 = 1.0f / w1;
        float oneOverW2 = 1.0f / w2;
        float factor0 = bary.x * oneOverW0;
        float factor1 = bary.y * oneOverW1;
        float factor2 = bary.z * oneOverW2;
        float invSum = 1.0f / (factor0 + factor1 + factor2);
        return (attr0 * factor0 + attr1 * factor1 + attr2 * factor2) * invSum;
    }

    void rasterizeTriangle(const VertexOut& v0, const VertexOut& v1, const VertexOut& v2,
        std::function<Color(const VertexOut&)> fragmentShader) {
        if (!m_frameBuffer) return;

        Vector2 s0 = toScreenSpace(v0);
        Vector2 s1 = toScreenSpace(v1);
        Vector2 s2 = toScreenSpace(v2);

        int minX = static_cast<int>(std::max(0.0f, std::min({ s0.x, s1.x, s2.x })));
        int maxX = static_cast<int>(std::min((float)m_width - 1, std::max({ s0.x, s1.x, s2.x })));
        int minY = static_cast<int>(std::max(0.0f, std::min({ s0.y, s1.y, s2.y })));
        int maxY = static_cast<int>(std::min((float)m_height - 1, std::max({ s0.y, s1.y, s2.y })));

        float w0 = v0.position.w;
        float w1 = v1.position.w;
        float w2 = v2.position.w;

        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                Vector2 p((float)x + 0.5f, (float)y + 0.5f);
                Vector3 bary = computeBarycentric(p, s0, s1, s2);

                if (bary.x >= -EPSILON && bary.y >= -EPSILON && bary.z >= -EPSILON) {
                    float depth = perspectiveCorrect<float>(bary, v0.position.z, v1.position.z, v2.position.z, w0, w1, w2);

                    if (!m_depthBuffer || m_depthBuffer->testAndSet(x, y, depth)) {
                        VertexOut frag;
                        frag.color = linearInterpolate<Color>(bary, v0.color, v1.color, v2.color);
                        frag.position = Vector4((float)x, (float)y, depth, 1.0f);
                        frag.worldPosition = v0.worldPosition * bary.x + v1.worldPosition * bary.y + v2.worldPosition * bary.z;
                        frag.normal = v0.normal * bary.x + v1.normal * bary.y + v2.normal * bary.z;
                        frag.texCoord = v0.texCoord * bary.x + v1.texCoord * bary.y + v2.texCoord * bary.z;

                        Color finalColor = fragmentShader(frag);
                        m_frameBuffer->setPixel(x, y, finalColor);
                    }
                }
            }
        }
    }
};
}
