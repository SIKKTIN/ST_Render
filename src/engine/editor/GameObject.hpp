#pragma once

#include <string>
#include <memory>
#include <vector>
#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"
#include "renderer/geometry/Vertex.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "renderer/buffer/Texture2D.hpp"

namespace ST {

class Script;
class GameObject;

enum class ShapeType {
    None,
    Quad,
    Cube,
    Circle,
    Triangle,
    Line
};

class GameObject {
public:
    std::string name;

    Vector3 position;
    Vector3 rotation;
    Vector3 scale;

    ShapeType shapeType;
    Color color;
    Color tint;
    int sortingOrder;
    bool showAABB;

    GameObject* parent = nullptr;
    std::vector<GameObject*> children;

    GameObject();
    explicit GameObject(const std::string& name);
    virtual ~GameObject();

    virtual void init();
    virtual void start();
    virtual void update(float deltaTime);
    virtual void destroy();

    bool hasInitialized() const { return m_initialized; }
    bool hasStarted() const { return m_started; }
    bool isRuntimeActive() const { return m_runtimeActive; }
    void markInitialized(bool value) { m_initialized = value; }
    void markStarted(bool value) { m_started = value; }
    void markRuntimeActive(bool value) { m_runtimeActive = value; }

    void addChild(GameObject* child);
    void removeChild(GameObject* child);
    void setParent(GameObject* newParent);
    bool isDescendantOf(const GameObject* ancestor) const;

    void setPosition(float x, float y, float z = 0.0f);
    void setPosition(const Vector3& pos);
    void setRotation(float x, float y, float z);
    void setRotation(const Vector3& rot);
    void setScale(float x, float y, float z = 1.0f);
    void setScale(const Vector3& sc);
    void setColor(float r, float g, float b, float a = 1.0f);
    void setSortingOrder(int order) { sortingOrder = order; }

    void setTexture(ST::Texture2D* tex) { m_texture = tex; }
    Texture2D* getTexture() const { return m_texture; }

    void setUserData(void* data) { m_userData = data; }
    void* getUserData() const { return m_userData; }

    // Script management
    void addScript(Script* script);
    void removeScript(Script* script);
    Script* getScript(const std::string& className) const;
    const std::vector<Script*>& getScripts() const { return m_scripts; }
    void clearScripts();

    void invokeInit();
    void invokeStart();
    void invokeUpdate(float deltaTime);
    void invokeDestroy();
    void invokeGameStart();
    void invokeGameStop();

    void createQuad(float width, float height);
    void createCube(float width, float height, float depth);
    void createCircle(float radius, int segments = 32);
    void createTriangle(float size);

    const Mesh& getMesh() const { return m_mesh; }
    ShapeType getShapeType() const { return shapeType; }
    void rebuildMesh();

    Mesh m_mesh;
    Texture2D* m_texture;
    bool m_dirty;
    void* m_userData = nullptr;
    std::vector<Script*> m_scripts;

protected:
    bool m_initialized = false;
    bool m_started = false;
    bool m_runtimeActive = false;
};

}
