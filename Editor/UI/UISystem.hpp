#pragma once

#include "UIBase.hpp"
#include "UIPanel.hpp"
#include "UILabel.hpp"
#include "UIButton.hpp"
#include <vector>

namespace ST {

class Scene;

class UISystem {
public:
    static UISystem& get();

    void setScene(Scene* scene);
    Scene* getScene() const { return m_scene; }

    void setScreenSize(float w, float h) { m_screenW = w; m_screenH = h; }
    float getScreenWidth() const { return m_screenW; }
    float getScreenHeight() const { return m_screenH; }

    // Factory - elements live in Scene
    UIPanel* createPanel(const std::string& name = "");
    UILabel* createLabel(const std::string& name = "", const std::string& text = "");
    UIButton* createButton(const std::string& name = "", const std::string& label = "");
    void destroyElement(UIBase* element);

    void clear();

    void update();
    void renderGameUI(void* renderer, void* target,
                      int canvasX, int canvasY, int canvasW, int canvasH) const;

    UIBase* hitTest(float x, float y) const;
    void onMouseDown(float x, float y);
    void onMouseUp(float x, float y);
    void onMouseMove(float x, float y);

private:
    Scene* m_scene = nullptr;
    float m_screenW = 0.0f;
    float m_screenH = 0.0f;
};

}
