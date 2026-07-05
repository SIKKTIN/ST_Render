#pragma once

#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <functional>

namespace ST {

class GameObject;

class Script {
public:
    virtual ~Script() = default;

    virtual void onInit() {}
    virtual void onStart() {}
    virtual void onUpdate(float deltaTime) {}
    virtual void onDestroy() {}
    virtual void onGameStart() {}
    virtual void onGameStop() {}
    virtual void renderInspector() {}

    virtual const char* getClassName() const = 0;
    const std::string& className() const { return m_className; }

    GameObject* gameObject() const { return m_gameObject; }
    void setEnabled(bool e) { m_enabled = e; }
    bool isEnabled() const { return m_enabled; }
    void attachTo(GameObject* go);

protected:
    std::string m_className;
    GameObject* m_gameObject = nullptr;
    bool m_enabled = true;
};

class ScriptRegistry {
public:
    static ScriptRegistry& instance();

    void registerScript(const std::string& className, std::function<Script*()> factory);
    Script* create(const std::string& className) const;
    const std::vector<std::string>& getAllScripts() const;

private:
    ScriptRegistry() = default;
    std::vector<std::string> m_names;
    std::vector<std::function<Script*()>> m_factories;
};

#define REGISTER_SCRIPT(ClassName) \
    static struct ClassName##_Reg { \
        ClassName##_Reg() { \
            ST::ScriptRegistry::instance().registerScript( \
                #ClassName, \
                []() -> ST::Script* { return new ClassName(); }); \
        } \
    } _reg_##ClassName

}
