#include "Script.hpp"
#include "GameObject.hpp"

namespace ST {

void Script::attachTo(GameObject* go) {
    m_gameObject = go;
}

ScriptRegistry& ScriptRegistry::instance() {
    static ScriptRegistry inst;
    return inst;
}

void ScriptRegistry::registerScript(const std::string& className, std::function<Script*()> factory) {
    m_names.push_back(className);
    m_factories.push_back(factory);
}

Script* ScriptRegistry::create(const std::string& className) const {
    for (size_t i = 0; i < m_names.size(); ++i) {
        if (m_names[i] == className) {
            return m_factories[i]();
        }
    }
    return nullptr;
}

const std::vector<std::string>& ScriptRegistry::getAllScripts() const {
    return m_names;
}

}
