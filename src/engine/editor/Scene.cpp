#include "engine/editor/Scene.hpp"
#include <algorithm>
#include <functional>

namespace ST {

Scene::Scene()
    : name("Untitled"), viewportOffset(0, 0, 0), viewportZoom(1.0f) {
}

Scene::Scene(const std::string& name)
    : name(name), viewportOffset(0, 0, 0), viewportZoom(1.0f) {
}

Scene::~Scene() {
    objects.clear();
}

Scene& Scene::operator=(Scene&& other) noexcept {
    if (this != &other) {
        name = std::move(other.name);
        camera = std::move(other.camera);
        viewportOffset = std::move(other.viewportOffset);
        viewportZoom = other.viewportZoom;
        objects = std::move(other.objects);
    }
    return *this;
}

GameObject* Scene::createGameObject(const std::string& name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    objects.push_back(std::move(obj));
    return ptr;
}

Sprite2D* Scene::createSprite2D(const std::string& name) {
    auto obj = std::make_unique<Sprite2D>(name);
    Sprite2D* ptr = obj.get();
    objects.push_back(std::move(obj));
    return ptr;
}

MusicPlayer* Scene::createMusicPlayer(const std::string& name) {
    auto obj = std::make_unique<MusicPlayer>(name);
    MusicPlayer* ptr = obj.get();
    objects.push_back(std::move(obj));
    return ptr;
}

Cinemachine* Scene::createCinemachine(const std::string& name) {
    auto obj = std::make_unique<Cinemachine>(name);
    Cinemachine* ptr = obj.get();
    objects.push_back(std::move(obj));
    return ptr;
}

void Scene::setParent(GameObject* child, GameObject* parent) {
    if (!child) return;
    child->setParent(parent);
}

void Scene::destroyGameObject(GameObject* obj) {
    if (!obj) return;

    std::vector<GameObject*> childrenToDelete = obj->children;
    for (GameObject* child : childrenToDelete) {
        destroyGameObject(child);
    }

    if (obj->parent) {
        obj->parent->removeChild(obj);
    }

    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (it->get() == obj) {
            objects.erase(it);
            return;
        }
    }
}

GameObject* Scene::findGameObject(const std::string& name) const {
    for (const auto& obj : objects) {
        if (obj->name == name) {
            return obj.get();
        }
    }
    return nullptr;
}

std::vector<GameObject*> Scene::getRootObjects() const {
    std::vector<GameObject*> roots;
    roots.reserve(objects.size());
    for (const auto& obj : objects) {
        if (!obj->parent) {
            roots.push_back(obj.get());
        }
    }
    return roots;
}

Cinemachine* Scene::findCinemachine() const {
    for (const auto& obj : objects) {
        if (auto* cm = dynamic_cast<Cinemachine*>(obj.get())) {
            return cm;
        }
    }
    return nullptr;
}

void Scene::startRuntime() {
    if (m_runtimeRunning) return;

    for (const auto& obj : objects) {
        GameObject* gameObject = obj.get();
        if (!gameObject->hasInitialized()) {
            gameObject->init();
            gameObject->invokeInit();
            gameObject->markInitialized(true);
        }
        if (!gameObject->hasStarted()) {
            gameObject->start();
            gameObject->invokeStart();
            gameObject->markStarted(true);
        }
        gameObject->markRuntimeActive(true);
        gameObject->invokeGameStart();
    }

    m_runtimeRunning = true;
}

void Scene::updateRuntime(float deltaTime) {
    if (!m_runtimeRunning) return;

    for (const auto& obj : objects) {
        GameObject* gameObject = obj.get();
        if (gameObject->isRuntimeActive()) {
            gameObject->update(deltaTime);
            gameObject->invokeUpdate(deltaTime);
        }
    }
}

void Scene::stopRuntime() {
    if (!m_runtimeRunning) return;

    for (const auto& obj : objects) {
        GameObject* gameObject = obj.get();
        if (gameObject->isRuntimeActive()) {
            gameObject->invokeGameStop();
            gameObject->invokeDestroy();
            gameObject->destroy();
            gameObject->markRuntimeActive(false);
            gameObject->markStarted(false);
        }
    }

    m_runtimeRunning = false;
}

Vector3 Scene::worldToScreen2D(const Vector3& worldPos, int screenW, int screenH) const {
    float x = (worldPos.x - viewportOffset.x) * viewportZoom + screenW * 0.5f;
    float y = (worldPos.y - viewportOffset.y) * viewportZoom + screenH * 0.5f;
    return Vector3(x, y, worldPos.z);
}

void Scene::pan2D(float dx, float dy) {
    viewportOffset.x -= dx / viewportZoom;
    viewportOffset.y -= dy / viewportZoom;
}

void Scene::zoom2D(float factor) {
    viewportZoom *= factor;
    if (viewportZoom < 0.01f) viewportZoom = 0.01f;
    if (viewportZoom > 100.0f) viewportZoom = 100.0f;
}

void Scene::zoom2DAt(float factor, float screenX, float screenY, int screenW, int screenH) {
    Vector3 worldBefore;
    worldBefore.x = (screenX - screenW * 0.5f) / viewportZoom + viewportOffset.x;
    worldBefore.y = (screenY - screenH * 0.5f) / viewportZoom + viewportOffset.y;

    viewportZoom *= factor;
    if (viewportZoom < 0.01f) viewportZoom = 0.01f;
    if (viewportZoom > 100.0f) viewportZoom = 100.0f;

    Vector3 worldAfter;
    worldAfter.x = (screenX - screenW * 0.5f) / viewportZoom + viewportOffset.x;
    worldAfter.y = (screenY - screenH * 0.5f) / viewportZoom + viewportOffset.y;

    viewportOffset.x += worldBefore.x - worldAfter.x;
    viewportOffset.y += worldBefore.y - worldAfter.y;
}

void Scene::getSortedObjects(std::vector<GameObject*>& out) const {
    out.clear();

    std::vector<GameObject*> roots = getRootObjects();
    std::stable_sort(roots.begin(), roots.end(),
        [](const GameObject* a, const GameObject* b) {
            return a->sortingOrder < b->sortingOrder;
        });

    std::function<void(GameObject*)> appendHierarchy = [&](GameObject* obj) {
        out.push_back(obj);

        std::vector<GameObject*> sortedChildren = obj->children;
        std::stable_sort(sortedChildren.begin(), sortedChildren.end(),
            [](const GameObject* a, const GameObject* b) {
                return a->sortingOrder < b->sortingOrder;
            });

        for (GameObject* child : sortedChildren) {
            appendHierarchy(child);
        }
    };

    for (GameObject* root : roots) {
        appendHierarchy(root);
    }
}

void Scene::applyCinemachineToViewport() {
    if (Cinemachine* cm = findCinemachine()) {
        viewportOffset.x = -cm->position.x;
        viewportOffset.y = -cm->position.y;
    }
}

}
