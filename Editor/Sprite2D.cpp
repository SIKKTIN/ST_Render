#include "Sprite2D.hpp"
#include "TextureManager.hpp"

namespace ST {

Sprite2D::Sprite2D()
    : GameObject(), m_textureIndex(-1), m_flipX(false), m_flipY(false) {
    createQuad(200.0f, 200.0f);
    setColor(1.0f, 1.0f, 1.0f, 1.0f);
}

Sprite2D::Sprite2D(const std::string& name)
    : GameObject(name), m_textureIndex(-1), m_flipX(false), m_flipY(false) {
    createQuad(200.0f, 200.0f);
    setColor(1.0f, 1.0f, 1.0f, 1.0f);
}

Sprite2D::~Sprite2D() {
}

void Sprite2D::setTint(const Color& c) {
    tint = c;
}

void Sprite2D::setTint(float r, float g, float b, float a) {
    tint.r = r;
    tint.g = g;
    tint.b = b;
    tint.a = a;
}

void Sprite2D::setTextureIndex(int idx) {
    m_textureIndex = idx;
    if (idx >= 0) {
        Texture2D* src = TextureManager::getInstance().getTexture(idx);
        if (src) {
            m_texture = *src;
            m_textureName = TextureManager::getInstance().getTextureName(idx);
        } else {
            m_textureName.clear();
        }
    } else {
        m_textureName.clear();
    }
}

}
