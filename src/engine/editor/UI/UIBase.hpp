#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ST {

struct UIPoint {
    float x = 0.0f, y = 0.0f;
    UIPoint() = default;
    UIPoint(float x_, float y_) : x(x_), y(y_) {}
};

struct UIRect {
    float x = 0.0f, y = 0.0f;
    float w = 100.0f, h = 100.0f;
    UIRect() = default;
    UIRect(float x_, float y_, float w_, float h_) : x(x_), y(y_), w(w_), h(h_) {}
    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

class UIBase {
public:
    enum class Anchor { TopLeft, TopCenter, TopRight,
                        MiddleLeft, MiddleCenter, MiddleRight,
                        BottomLeft, BottomCenter, BottomRight };

    explicit UIBase(const std::string& name);
    virtual ~UIBase() = default;

    const std::string& getName() const { return m_name; }

    const UIPoint& getPosition() const { return m_position; }
    float getWidth() const { return m_width; }
    float getHeight() const { return m_height; }

    void setPosition(float x, float y);
    void setSize(float w, float h);
    void setAnchor(Anchor anchor);
    void setParent(UIBase* parent) { m_parent = parent; }
    UIBase* getParent() const { return m_parent; }

    UIRect getWorldRect() const;

    virtual void update() {}
    virtual void render() const {}
    virtual void onMouseDown(float x, float y) {}
    virtual void onMouseUp(float x, float y) {}
    virtual void onMouseMove(float x, float y) {}

    const std::vector<UIBase*>& getChildren() const { return m_children; }
    void addChild(UIBase* child);
    void removeChild(UIBase* child);

protected:
    std::string m_name;
    UIPoint m_position;
    float m_width = 0.0f;
    float m_height = 0.0f;
    Anchor m_anchor = Anchor::TopLeft;
    UIBase* m_parent = nullptr;
    std::vector<UIBase*> m_children;

    UIPoint screenToLocal(float sx, float sy) const;
    UIRect anchorToRect(float parentW, float parentH) const;
};

}
