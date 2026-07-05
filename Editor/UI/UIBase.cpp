#include "UIBase.hpp"

namespace ST {

UIBase::UIBase(const std::string& name) : m_name(name) {}

void UIBase::setPosition(float x, float y) {
    m_position.x = x;
    m_position.y = y;
}

void UIBase::setSize(float w, float h) {
    m_width = w;
    m_height = h;
}

void UIBase::setAnchor(Anchor anchor) {
    m_anchor = anchor;
}

UIRect UIBase::anchorToRect(float parentW, float parentH) const {
    float ax = 0.0f, ay = 0.0f;
    switch (m_anchor) {
        case Anchor::TopLeft:     case Anchor::MiddleLeft:     case Anchor::BottomLeft:     ax = 0.0f; break;
        case Anchor::TopCenter:   case Anchor::MiddleCenter:  case Anchor::BottomCenter:  ax = 0.5f; break;
        case Anchor::TopRight:    case Anchor::MiddleRight:   case Anchor::BottomRight:   ax = 1.0f; break;
    }
    switch (m_anchor) {
        case Anchor::TopLeft:    case Anchor::TopCenter:    case Anchor::TopRight:    ay = 0.0f; break;
        case Anchor::MiddleLeft: case Anchor::MiddleCenter: case Anchor::MiddleRight: ay = 0.5f; break;
        case Anchor::BottomLeft: case Anchor::BottomCenter: case Anchor::BottomRight: ay = 1.0f; break;
    }
    return UIRect(m_position.x - ax * m_width, m_position.y - ay * m_height, m_width, m_height);
}

UIRect UIBase::getWorldRect() const {
    float pw = 0.0f, ph = 0.0f;
    if (m_parent) {
        UIRect pr = m_parent->getWorldRect();
        pw = pr.w;
        ph = pr.h;
    }
    UIRect rect = anchorToRect(pw, ph);
    if (m_parent) {
        UIRect pr = m_parent->getWorldRect();
        rect.x += pr.x;
        rect.y += pr.y;
    }
    return rect;
}

UIPoint UIBase::screenToLocal(float sx, float sy) const {
    UIRect r = getWorldRect();
    return UIPoint(sx - r.x, sy - r.y);
}

void UIBase::addChild(UIBase* child) {
    if (child && child->m_parent != this) {
        if (child->m_parent) child->m_parent->removeChild(child);
        m_children.push_back(child);
        child->m_parent = this;
    }
}

void UIBase::removeChild(UIBase* child) {
    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i] == child) {
            m_children.erase(m_children.begin() + i);
            child->m_parent = nullptr;
            return;
        }
    }
}

}
