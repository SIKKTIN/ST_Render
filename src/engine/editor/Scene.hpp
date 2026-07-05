#pragma once

#include <string>
#include <vector>
#include <memory>
#include "engine/editor/GameObject.hpp"
#include "engine/editor/Sprite2D.hpp"
#include "engine/editor/MusicPlayer.hpp"
#include "engine/editor/Cinemachine.hpp"
#include "engine/editor/ITransformUI.hpp"
#include "core/camera/Camera.hpp"
#include "core/math/Vector3.hpp"

namespace ST {

class Scene {
public:
    std::string name;
    Camera camera;

    Vector3 viewportOffset;
    float viewportZoom;

    Scene();
    explicit Scene(const std::string& name);
    ~Scene();

    GameObject* createGameObject(const std::string& name = "GameObject");
    Sprite2D* createSprite2D(const std::string& name = "Sprite2D");
    MusicPlayer* createMusicPlayer(const std::string& name = "MusicPlayer");
    Cinemachine* createCinemachine(const std::string& name = "Cinemachine");
    void setParent(GameObject* child, GameObject* parent);
    void destroyGameObject(GameObject* obj);
    GameObject* findGameObject(const std::string& name) const;
    Cinemachine* findCinemachine() const;

    void startRuntime();
    void updateRuntime(float deltaTime);
    void stopRuntime();
    bool isRuntimeRunning() const { return m_runtimeRunning; }

    const std::vector<std::unique_ptr<GameObject>>& getObjects() const { return objects; }
    std::vector<GameObject*> getRootObjects() const;
    size_t getObjectCount() const { return objects.size(); }

    void getSortedObjects(std::vector<GameObject*>& out) const;

    void setTransformUI(ITransformUI* ui) { m_transformUI = ui; }
    Scene& operator=(Scene&& other) noexcept;
    bool renderTransformControls(GameObject* obj) {
        return m_transformUI ? m_transformUI->renderControls(obj) : false;
    }

    Vector3 worldToScreen2D(const Vector3& worldPos, int screenW, int screenH) const;

    void pan2D(float dx, float dy);
    void zoom2D(float factor);
    void zoom2DAt(float factor, float screenX, float screenY, int screenW, int screenH);

    void applyCinemachineToViewport();

private:
    std::vector<std::unique_ptr<GameObject>> objects;
    ITransformUI* m_transformUI = nullptr;
    bool m_runtimeRunning = false;
};

}
