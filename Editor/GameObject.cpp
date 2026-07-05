#include "GameObject.hpp"
#include "Script.hpp"
#include <algorithm>
#include <cmath>

namespace ST {

GameObject::GameObject()
    : position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1),
      shapeType(ShapeType::None), color(1, 1, 1, 1), tint(1, 1, 1, 1), sortingOrder(0), showAABB(false), m_texture(nullptr), m_dirty(false), m_userData(nullptr) {
}

GameObject::GameObject(const std::string& name)
    : name(name), position(0, 0, 0), rotation(0, 0, 0), scale(1, 1, 1),
      shapeType(ShapeType::None), color(1, 1, 1, 1), tint(1, 1, 1, 1), sortingOrder(0), showAABB(false), m_texture(nullptr), m_dirty(false), m_userData(nullptr) {
}

GameObject::~GameObject() {
    clearScripts();
}

void GameObject::init() {
}

void GameObject::start() {
}

void GameObject::update(float deltaTime) {
    (void)deltaTime;
}

void GameObject::destroy() {
}

void GameObject::addChild(GameObject* child) {
    if (!child || child == this) return;
    if (child->parent == this) return;
    child->setParent(this);
}

void GameObject::removeChild(GameObject* child) {
    if (!child) return;
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
        if (child->parent == this) {
            child->parent = nullptr;
        }
    }
}

void GameObject::setParent(GameObject* newParent) {
    if (newParent == this) return;
    if (newParent && newParent->isDescendantOf(this)) return;
    if (parent == newParent) return;

    if (parent) {
        auto it = std::find(parent->children.begin(), parent->children.end(), this);
        if (it != parent->children.end()) {
            parent->children.erase(it);
        }
    }

    parent = newParent;
    if (parent) {
        auto it = std::find(parent->children.begin(), parent->children.end(), this);
        if (it == parent->children.end()) {
            parent->children.push_back(this);
        }
    }
}

bool GameObject::isDescendantOf(const GameObject* ancestor) const {
    const GameObject* current = parent;
    while (current) {
        if (current == ancestor) return true;
        current = current->parent;
    }
    return false;
}

void GameObject::setPosition(float x, float y, float z) {
    position.x = x;
    position.y = y;
    position.z = z;
}

void GameObject::setPosition(const Vector3& pos) {
    position = pos;
}

void GameObject::setRotation(float x, float y, float z) {
    rotation.x = x;
    rotation.y = y;
    rotation.z = z;
}

void GameObject::setRotation(const Vector3& rot) {
    rotation = rot;
}

void GameObject::setScale(float x, float y, float z) {
    scale.x = x;
    scale.y = y;
    scale.z = z;
}

void GameObject::setScale(const Vector3& sc) {
    scale = sc;
}

void GameObject::setColor(float r, float g, float b, float a) {
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    m_dirty = true;
}

void GameObject::rebuildMesh() {
    m_mesh.clear();

    switch (shapeType) {
        case ShapeType::Quad:
        {
            float hw = 50.0f;
            float hh = 50.0f;
            m_mesh.addVertex(Vertex(Vector3(-hw, -hh, 0), Vector3(0,0,1), Vector2(0,0), color));
            m_mesh.addVertex(Vertex(Vector3( hw, -hh, 0), Vector3(0,0,1), Vector2(1,0), color));
            m_mesh.addVertex(Vertex(Vector3( hw,  hh, 0), Vector3(0,0,1), Vector2(1,1), color));
            m_mesh.addVertex(Vertex(Vector3(-hw,  hh, 0), Vector3(0,0,1), Vector2(0,1), color));
            m_mesh.addTriangle(0, 1, 2);
            m_mesh.addTriangle(0, 2, 3);
            break;
        }
        case ShapeType::Cube:
        {
            float hw = scale.x * 0.5f, hh = scale.y * 0.5f, hd = scale.z * 0.5f;
            Vector3 c[8] = {
                Vector3(-hw,-hh,-hd), Vector3( hw,-hh,-hd),
                Vector3( hw, hh,-hd), Vector3(-hw, hh,-hd),
                Vector3(-hw,-hh, hd), Vector3( hw,-hh, hd),
                Vector3( hw, hh, hd), Vector3(-hw, hh, hd),
            };
            int f[12][3] = {
                {0,1,2},{0,2,3},
                {5,4,7},{5,7,6},
                {4,0,3},{4,3,7},
                {1,5,6},{1,6,2},
                {3,2,6},{3,6,7},
                {4,5,1},{4,1,0}
            };
            Vector3 n[6] = {
                Vector3(0,0,-1), Vector3(0,0,1),
                Vector3(-1,0,0), Vector3(1,0,0),
                Vector3(0,1,0), Vector3(0,-1,0)
            };
            Vector2 u[4] = { Vector2(0,0), Vector2(1,0), Vector2(1,1), Vector2(0,1) };
            for (int fi = 0; fi < 6; ++fi) {
                int base = m_mesh.getVertexCount();
                for (int k = 0; k < 4; ++k) {
                    m_mesh.addVertex(Vertex(c[f[fi*2][k]], n[fi], u[k], color));
                }
                m_mesh.addTriangle(base+0, base+1, base+2);
                m_mesh.addTriangle(base+0, base+2, base+3);
            }
            break;
        }
        case ShapeType::Circle:
        {
            m_mesh.addVertex(Vertex(Vector3(0,0,0), Vector3(0,0,1), Vector2(0.5f,0.5f), color));
            for (int i = 0; i < 32; ++i) {
                float a = 2.0f * 3.1415926535f * i / 32.0f;
                float x = std::cos(a) * scale.x;
                float y = std::sin(a) * scale.x;
                m_mesh.addVertex(Vertex(Vector3(x,y,0), Vector3(0,0,1), Vector2(x/scale.x+0.5f, y/scale.x+0.5f), color));
            }
            for (int i = 0; i < 32; ++i) {
                m_mesh.addTriangle(0, i+1, ((i+1)%32)+1);
            }
            break;
        }
        case ShapeType::Triangle:
        {
            m_mesh.addVertex(Vertex(Vector3(0, 50.0f, 0), Vector3(0,0,1), Vector2(0.5f,1), color));
            m_mesh.addVertex(Vertex(Vector3(-50.0f,-50.0f,0), Vector3(0,0,1), Vector2(0,0), color));
            m_mesh.addVertex(Vertex(Vector3( 50.0f,-50.0f,0), Vector3(0,0,1), Vector2(1,0), color));
            m_mesh.addTriangle(0, 1, 2);
            break;
        }
        default:
            break;
    }
    m_dirty = false;
}

void GameObject::createQuad(float width, float height) {
    shapeType = ShapeType::Quad;
    scale.x = 1.0f;
    scale.y = 1.0f;
    scale.z = 1.0f;
    rebuildMesh();
}

void GameObject::createCube(float width, float height, float depth) {
    shapeType = ShapeType::Cube;
    setScale(width, height, depth);
    rebuildMesh();
}

void GameObject::createCircle(float radius, int segments) {
    shapeType = ShapeType::Circle;
    setScale(radius, radius, 1.0f);
    rebuildMesh();
}

void GameObject::addScript(Script* script) {
    if (!script) return;
    m_scripts.push_back(script);
    script->attachTo(this);
}

void GameObject::removeScript(Script* script) {
    if (!script) return;
    auto it = std::find(m_scripts.begin(), m_scripts.end(), script);
    if (it != m_scripts.end()) m_scripts.erase(it);
}

Script* GameObject::getScript(const std::string& className) const {
    for (Script* s : m_scripts) {
        if (s && typeid(*s).name() == className) return s;
    }
    return nullptr;
}

void GameObject::clearScripts() {
    for (Script* s : m_scripts) delete s;
    m_scripts.clear();
}

void GameObject::invokeInit() {
    for (Script* s : m_scripts) if (s && s->isEnabled()) s->onInit();
}

void GameObject::invokeStart() {
    for (Script* s : m_scripts) if (s && s->isEnabled()) s->onStart();
}

void GameObject::invokeUpdate(float deltaTime) {
    for (Script* s : m_scripts) if (s && s->isEnabled()) s->onUpdate(deltaTime);
}

void GameObject::invokeDestroy() {
    for (Script* s : m_scripts) if (s) s->onDestroy();
}

void GameObject::invokeGameStart() {
    for (Script* s : m_scripts) if (s && s->isEnabled()) s->onGameStart();
}

void GameObject::invokeGameStop() {
    for (Script* s : m_scripts) if (s && s->isEnabled()) s->onGameStop();
}

void GameObject::createTriangle(float size) {
    shapeType = ShapeType::Triangle;
    rebuildMesh();
}

}
