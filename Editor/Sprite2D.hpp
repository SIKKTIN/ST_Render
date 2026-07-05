#pragma once

#include "GameObject.hpp"
#include "../Buffer/Texture2D.hpp"

namespace ST {
class TextureManager;

class Sprite2D : public GameObject {
public:
    Sprite2D();
    explicit Sprite2D(const std::string& name);
    ~Sprite2D();

    void setTexture(Texture2D* tex) { m_texture = tex ? *tex : Texture2D(); }
    Texture2D& getTexture() { return m_texture; }
    const Texture2D& getTexture() const { return m_texture; }
    bool hasValidTexture() const { return m_texture.isValid(); }

    void setTextureName(const std::string& name) { m_textureName = name; }
    const std::string& getTextureName() const { return m_textureName; }

    int getTextureIndex() const { return m_textureIndex; }
    void setTextureIndex(int idx);

    void setFlipX(bool v) { m_flipX = v; }
    bool getFlipX() const { return m_flipX; }

    void setFlipY(bool v) { m_flipY = v; }
    bool getFlipY() const { return m_flipY; }

    void setTint(const Color& c);
    void setTint(float r, float g, float b, float a);

private:
    Texture2D m_texture;
    std::string m_textureName;
    int m_textureIndex;
    bool m_flipX;
    bool m_flipY;
};

}
