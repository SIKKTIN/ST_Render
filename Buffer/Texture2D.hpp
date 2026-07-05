#pragma once

#include "Vector4.hpp"
#include "../Math/Vector2.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

namespace ST {

enum class TextureWrap {
    Repeat,
    Clamp
};

enum class TextureFilter {
    Nearest,
    Bilinear
};

class Texture2D {
public:
    Texture2D() : m_width(0), m_height(0), m_wrap(TextureWrap::Repeat), m_filter(TextureFilter::Bilinear) {}

    void setPixels(const std::vector<Color>& pixels, int width, int height) {
        m_pixels = pixels;
        m_width = width;
        m_height = height;
    }

    Color sample(float u, float v) const {
        return sampleImpl(u, v);
    }

    void setWrap(TextureWrap mode) { m_wrap = mode; }
    void setFilter(TextureFilter mode) { m_filter = mode; }
    TextureWrap getWrap() const { return m_wrap; }
    TextureFilter getFilter() const { return m_filter; }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    bool isValid() const { return !m_pixels.empty(); }
    const std::vector<Color>& getPixels() const { return m_pixels; }

private:
    Color sampleImpl(float u, float v) const {
        if (!isValid()) return Color::white();

        float s = u;
        float t = v;

        if (m_wrap == TextureWrap::Repeat) {
            s = s - std::floor(s);
            t = t - std::floor(t);
        } else {
            s = std::max(0.0f, std::min(1.0f, s));
            t = std::max(0.0f, std::min(1.0f, t));
        }

        if (m_filter == TextureFilter::Nearest) {
            int px = static_cast<int>(s * (m_width - 1));
            int py = static_cast<int>(t * (m_height - 1));
            return getPixel(px, py);
        }

        float px_f = s * (m_width - 1);
        float py_f = t * (m_height - 1);
        int px0 = static_cast<int>(std::floor(px_f));
        int py0 = static_cast<int>(std::floor(py_f));
        int px1 = std::min(px0 + 1, m_width - 1);
        int py1 = std::min(py0 + 1, m_height - 1);

        float fx = px_f - px0;
        float fy = py_f - py0;

        Color c00 = getPixel(px0, py0);
        Color c10 = getPixel(px1, py0);
        Color c01 = getPixel(px0, py1);
        Color c11 = getPixel(px1, py1);

        Color c0 = Color::lerp(c00, c10, fx);
        Color c1 = Color::lerp(c01, c11, fx);
        return Color::lerp(c0, c1, fy);
    }

    Color getPixel(int x, int y) const {
        if (x < 0 || x >= m_width || y < 0 || y >= m_height) return Color(0, 0, 0, 0);
        return m_pixels[y * m_width + x];
    }

    int m_width;
    int m_height;
    std::vector<Color> m_pixels;
    TextureWrap m_wrap;
    TextureFilter m_filter;
};

}
