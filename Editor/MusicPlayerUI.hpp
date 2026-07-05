#pragma once

#include "TransformUI_ImGui.hpp"
#include "MusicPlayer.hpp"
#include <imgui.h>
#include <vector>
#include <string>

namespace ST {

class MusicPlayerUI : public TransformUI_ImGui {
public:
    bool renderControls(GameObject* obj) override {
        bool changed = TransformUI_ImGui::renderControls(obj);

        if (auto* player = dynamic_cast<MusicPlayer*>(obj)) {
            changed = renderMusicPlayerControls(player) || changed;
        }
        return changed;
    }

private:
    bool renderMusicPlayerControls(MusicPlayer* player) {
        bool changed = false;

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.6f, 1.0f, 1.0f), "MusicPlayer");

        char pathBuf[256];
        strncpy_s(pathBuf, player->getAudioPath().c_str(), sizeof(pathBuf) - 1);
        pathBuf[sizeof(pathBuf) - 1] = '\0';
        if (ImGui::InputText("Audio Path", pathBuf, sizeof(pathBuf))) {
            player->setAudioPath(pathBuf);
            changed = true;
        }
        ImGui::TextDisabled("Relative to Data/Audio/ (e.g. mario.mp3)");

        ImGui::Spacing();
        ImGui::Text("Volume");
        int vol = player->getVolume();
        if (ImGui::SliderInt("##vol", &vol, 0, 100, "%d%%", ImGuiSliderFlags_AlwaysClamp)) {
            player->setVolume(vol);
            changed = true;
        }

        bool loop = player->getLoop();
        if (ImGui::Checkbox("Loop", &loop)) {
            player->setLoop(loop);
            changed = true;
        }

        bool autoPlay = player->getAutoPlay();
        if (ImGui::Checkbox("Auto Play", &autoPlay)) {
            player->setAutoPlay(autoPlay);
            changed = true;
        }

        return changed;
    }
};

} // namespace ST
