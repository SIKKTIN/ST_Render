#pragma once

#include <string>
#include <vector>
#include <fstream>

namespace ST {

class GameObject;
class Scene;

class SceneData {
public:
    static bool saveScene(const Scene& scene, const std::string& filePath);
    static bool loadScene(Scene& scene, const std::string& filePath);

private:
    static std::string escapeString(const std::string& s);
    static void writeIndent(std::ofstream& fout, int indent);
};

}
