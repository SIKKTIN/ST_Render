#include "SceneData.hpp"
#include "../../Editor/Scene.hpp"
#include "../../Editor/GameObject.hpp"
#include "../../Editor/Sprite2D.hpp"
#include "../../Editor/MusicPlayer.hpp"
#include "../../Editor/Cinemachine.hpp"
#include "../../Math/Vector3.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstdio>

using json = nlohmann::json;

namespace ST {

// Escape " and \ in JSON strings
std::string SceneData::escapeString(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') r.push_back('\\');
        r.push_back(c);
    }
    return r;
}

void SceneData::writeIndent(std::ofstream& fout, int indent) {
    for (int i = 0; i < indent; i++) fout << "    ";
}

bool SceneData::saveScene(const Scene& scene, const std::string& filePath) {
    std::ofstream fout(filePath);
    if (!fout.is_open()) return false;

    fout << "{\n";
    fout << "    \"sceneName\": \"" << escapeString(scene.name) << "\",\n";

    const char* shapeNames[] = { "None", "Quad", "Cube", "Circle", "Triangle", "Line" };

    auto& objs = scene.getObjects();
    fout << "    \"objects\": [\n";

    for (size_t i = 0; i < objs.size(); ++i) {
        const GameObject* obj = objs[i].get();
        const Sprite2D* sp = dynamic_cast<const Sprite2D*>(obj);
        const MusicPlayer* mp = dynamic_cast<const MusicPlayer*>(obj);
        const Cinemachine* cm = dynamic_cast<const Cinemachine*>(obj);

        fout << "        {\n";
        fout << "            \"name\": \"" << escapeString(obj->name) << "\",\n";

        // transform
        fout << "            \"position\": [" << obj->position.x << ", " << obj->position.y << ", " << obj->position.z << "],\n";
        fout << "            \"rotation\": [" << obj->rotation.x << ", " << obj->rotation.y << ", " << obj->rotation.z << "],\n";
        fout << "            \"scale\": [" << obj->scale.x << ", " << obj->scale.y << ", " << obj->scale.z << "],\n";

        // shape type
        int stIdx = static_cast<int>(obj->shapeType);
        const char* stName = (stIdx >= 0 && stIdx <= 5) ? shapeNames[stIdx] : "None";
        fout << "            \"shapeType\": \"" << stName << "\",\n";

        // color
        fout << "            \"color\": [" << obj->color.r << ", " << obj->color.g << ", " << obj->color.b << ", " << obj->color.a << "],\n";

        // tint
        fout << "            \"tint\": [" << obj->tint.r << ", " << obj->tint.g << ", " << obj->tint.b << ", " << obj->tint.a << "],\n";

        fout << "            \"sortingOrder\": " << obj->sortingOrder << ",\n";
        fout << "            \"showAABB\": " << (obj->showAABB ? "true" : "false") << ",\n";
        fout << "            \"isCinemachine\": " << (cm ? "true" : "false") << ",\n";

        // sprite2d extras
        if (sp) {
            fout << "            \"isSprite\": true,\n";
            fout << "            \"textureName\": \"" << escapeString(sp->getTextureName()) << "\",\n";
            fout << "            \"textureIndex\": " << sp->getTextureIndex() << ",\n";
            fout << "            \"flipX\": " << (sp->getFlipX() ? "true" : "false") << ",\n";
            fout << "            \"flipY\": " << (sp->getFlipY() ? "true" : "false") << "\n";
        } else if (mp) {
            fout << "            \"isMusicPlayer\": true,\n";
            fout << "            \"audioIndex\": " << mp->getAudioIndex() << ",\n";
            fout << "            \"audioPath\": \"" << escapeString(mp->getAudioPath()) << "\",\n";
            fout << "            \"volume\": " << mp->getVolume() << ",\n";
            fout << "            \"loop\": " << (mp->getLoop() ? "true" : "false") << ",\n";
            fout << "            \"autoPlay\": " << (mp->getAutoPlay() ? "true" : "false") << "\n";
        } else if (cm) {
            fout << "            \"cinemachine\": {\n";
            fout << "                \"viewportWidth\": " << cm->getViewportWidth() << "\n";
            fout << "            }\n";
        } else {
            fout << "            \"isSprite\": false\n";
        }

        fout << "        }" << (i + 1 < objs.size() ? "," : "") << "\n";
    }

    fout << "    ]\n";
    fout << "}\n";
    return true;
}

bool SceneData::loadScene(Scene& scene, const std::string& filePath) {
    std::ifstream fin(filePath);
    if (!fin.is_open()) return false;

    json j;
    fin >> j;

    scene.name = j["sceneName"].get<std::string>();

    if (j.contains("objects")) {
        for (auto& item : j["objects"]) {
        std::string objName = item["name"].get<std::string>();
        bool isSprite = item.value("isSprite", false);
        bool isMusicPlayer = item.value("isMusicPlayer", false);
        bool isCinemachine = item.value("isCinemachine", false);

        GameObject* obj = nullptr;
        if (isMusicPlayer) {
            obj = scene.createMusicPlayer(objName);
        } else if (isSprite) {
            obj = scene.createSprite2D(objName);
        } else if (isCinemachine) {
            obj = scene.createCinemachine(objName);
        } else {
            obj = scene.createGameObject(objName);
        }

        auto posArr = item["position"];
        obj->position = Vector3(posArr[0], posArr[1], posArr[2]);

        auto rotArr = item["rotation"];
        obj->rotation = Vector3(rotArr[0], rotArr[1], rotArr[2]);

        auto scaleArr = item["scale"];
        obj->scale = Vector3(scaleArr[0], scaleArr[1], scaleArr[2]);

        std::string shapeName = item["shapeType"].get<std::string>();
        if (shapeName == "Quad") {
            obj->shapeType = ShapeType::Quad;
        } else if (shapeName == "Cube") {
            obj->shapeType = ShapeType::Cube;
        } else if (shapeName == "Circle") {
            obj->shapeType = ShapeType::Circle;
        } else if (shapeName == "Triangle") {
            obj->shapeType = ShapeType::Triangle;
        }
        obj->rebuildMesh();

        auto colArr = item["color"];
        obj->color = Color(colArr[0], colArr[1], colArr[2], colArr[3]);

        auto tintArr = item["tint"];
        obj->tint = Color(tintArr[0], tintArr[1], tintArr[2], tintArr[3]);

        obj->sortingOrder = item["sortingOrder"].get<int>();
        obj->showAABB = item["showAABB"].get<bool>();

        if (isCinemachine) {
            if (auto* cm = dynamic_cast<Cinemachine*>(obj)) {
                if (item.contains("cinemachine")) {
                    const auto& cmData = item["cinemachine"];
                    cm->setViewportWidth(cmData.value("viewportWidth", obj->scale.x));
                } else {
                    cm->setViewportWidth(obj->scale.x);
                }
            }
        }

        if (isSprite) {
            Sprite2D* sp = static_cast<Sprite2D*>(obj);
            sp->setTextureIndex(item["textureIndex"].get<int>());
            sp->setFlipX(item["flipX"].get<bool>());
            sp->setFlipY(item["flipY"].get<bool>());
        } else if (isMusicPlayer) {
            MusicPlayer* mp = static_cast<MusicPlayer*>(obj);
            mp->setAudioIndex(item.value("audioIndex", -1));
            mp->setAudioPath(item.value("audioPath", ""));
            mp->setVolume(item.value("volume", 80));
            mp->setLoop(item.value("loop", true));
            mp->setAutoPlay(item.value("autoPlay", false));
        }

        printf("[LOAD] obj='%s' shape='%s' pos=(%.1f,%.1f,%.1f) scale=(%.1f,%.1f,%.1f) isCinemachine=%s\n",
               objName.c_str(), shapeName.c_str(),
               obj->position.x, obj->position.y, obj->position.z,
               obj->scale.x, obj->scale.y, obj->scale.z,
               dynamic_cast<Cinemachine*>(obj) ? "true" : "false");
    }

    }

    return true;
}

}
