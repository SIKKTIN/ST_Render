file(GLOB ST_SCRIPTS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Scripts/*.cpp")

add_library(Editor
    GameObject.cpp
    Scene.cpp
    Sprite2D.cpp
    Script.cpp
    MusicPlayer.cpp
    TextureManager.cpp
    AudioManager.cpp
    Cinemachine.cpp
    NetWork/NetworkDiscovery.cpp
    UI/UIBase.cpp
    UI/UIWidget.cpp
    UI/UIPanel.cpp
    UI/UIEditorPanel.cpp
    UI/UILabel.cpp
    UI/UIButton.cpp
    UI/UIImage.cpp
    ${ST_SCRIPTS_SOURCES}
)

target_include_directories(Editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Editor PUBLIC
    Math
    Camera
    Buffer
    Texture
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)
