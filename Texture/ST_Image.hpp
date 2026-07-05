#pragma once

#include "stb_image.h"
#include "../Math/Vector4.hpp"
#include <string>
#include <vector>

namespace ST {

class Image {
public:
    Image() : m_width(0), m_height(0), m_channels(0) {}

    bool load(const char* path) {
        m_pixels.clear();
        stbi_set_flip_vertically_on_load(true);
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 3);
        if (!data) return false;

        m_width = w;
        m_height = h;
        m_channels = ch;

        m_pixels.resize(w * h);
        for (int y = 0; y < h; y++) {
            int dst = y * w;
            int src = (h - 1 - y) * w;
            for (int x = 0; x < w; x++) {
                float r = data[(src + x) * 3 + 0] / 255.0f;
                float g = data[(src + x) * 3 + 1] / 255.0f;
                float b = data[(src + x) * 3 + 2] / 255.0f;
                m_pixels[dst + x] = Color(r, g, b, 1.0f);
            }
        }

        stbi_image_free(data);
        return true;
    }

    void clear() {
        m_pixels.clear();
        m_width = 0;
        m_height = 0;
        m_channels = 0;
    }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getChannels() const { return m_channels; }
    const std::vector<Color>& getPixels() const { return m_pixels; }

    bool isValid() const { return !m_pixels.empty(); }

private:
    int m_width;
    int m_height;
    int m_channels;
    std::vector<Color> m_pixels;
};

}
