#pragma once

#include <vector>

struct SDL_Renderer;
struct SDL_Texture;

namespace ST {

class Scene;
class UIPanel;

class UICanvas {
public:
    static UICanvas& get();

    void setScene(Scene* scene);
    void addPanel(UIPanel* panel, int sortingOrder = 0);
    void removePanel(UIPanel* panel);
    void clear();

    void update();

    void render(SDL_Renderer* renderer, SDL_Texture* target,
                int canvasX, int canvasY, int canvasW, int canvasH) const;

    void handleMouseDown(int x, int y, int canvasX, int canvasY);
    void handleMouseUp(int x, int y, int canvasX, int canvasY);
    void handleMouseMove(int x, int y, int canvasX, int canvasY);

private:
    UICanvas() : m_scene(nullptr) {}

    struct PanelEntry {
        UIPanel* panel;
        int sortingOrder;
    };
    Scene* m_scene = nullptr;
    std::vector<PanelEntry> m_panels;
};

}
