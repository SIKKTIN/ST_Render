#pragma once

#include "stb_image.h"
#include "core/math/Vector4.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ST {

class Image {
public:
    Image() : m_width(0), m_height(0), m_channels(0) {}

    bool load(const char* path) {
        return load(path, 0, true);
    }

    // Load an image, optionally limiting its largest dimension.  The software
    // renderer stores decoded pixels as float Colors, so keeping very large
    // source maps at their original resolution can consume hundreds of MB per
    // texture.  A zero maxDimension preserves the original behaviour.
    bool load(const char* path, int maxDimension) {
        return load(path, maxDimension, true);
    }

    // Some FBX exports keep texture V coordinates in the source-image
    // orientation. Callers can opt out of the repository-wide vertical flip
    // when the asset's UV convention is known explicitly.
    bool load(const char* path, int maxDimension, bool flipVertical) {
        m_pixels.clear();
        stbi_set_flip_vertically_on_load(flipVertical ? 1 : 0);
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 3);
        if (!data) return false;

        const int sourceWidth = w;
        const int sourceHeight = h;
        const float scale = (maxDimension > 0)
            ? std::min(1.0f, static_cast<float>(maxDimension) /
                                  static_cast<float>(std::max(w, h)))
            : 1.0f;
        const int targetWidth = std::max(1, static_cast<int>(std::lround(w * scale)));
        const int targetHeight = std::max(1, static_cast<int>(std::lround(h * scale)));

        m_width = targetWidth;
        m_height = targetHeight;
        m_channels = ch;

        m_pixels.resize(targetWidth * targetHeight);
        for (int y = 0; y < targetHeight; y++) {
            // Flip vertically while sampling the source image.  Nearest
            // sampling keeps this load path inexpensive; bilinear filtering
            // is still applied by the renderer at draw time.
            const int sourceY = std::min(sourceHeight - 1,
                static_cast<int>((static_cast<float>(y) + 0.5f) * sourceHeight /
                                 targetHeight));
            const int src = (sourceHeight - 1 - sourceY) * sourceWidth;
            const int dst = y * targetWidth;
            for (int x = 0; x < targetWidth; x++) {
                const int sourceX = std::min(sourceWidth - 1,
                    static_cast<int>((static_cast<float>(x) + 0.5f) * sourceWidth /
                                     targetWidth));
                const int pixel = (src + sourceX) * 3;
                float r = data[pixel + 0] / 255.0f;
                float g = data[pixel + 1] / 255.0f;
                float b = data[pixel + 2] / 255.0f;
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
