# src/app/main/CMakeLists.txt
# 主程序和测试管理器（ImGui）

# ST_Render_Manager (Dear ImGui)
# 主入口：test_manager_main.cpp 负责启动 ImGui 框架
add_executable(ST_Render_Manager
    test_manager_main.cpp
    HierarchyPanel.cpp
    TestModule_FrameBuffer.cpp
    TestModule_Rasterizer.cpp
    TestModule_3DRender.cpp
    TestModule_Texture.cpp
    TestModule_Music.cpp
    TestModule_NetworkTest.cpp
    imgui_impl_sdl2.cpp
    imgui_impl_sdlrenderer2.cpp
)

target_link_libraries(ST_Render_Manager PRIVATE
    Math Buffer Geometry Transform Camera Pipeline Renderer Editor DataBase Music
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    imgui::imgui
)

target_link_options(ST_Render_Manager PRIVATE /OPT:NOREF /WHOLEARCHIVE:$<TARGET_FILE:Editor> /WHOLEARCHIVE:$<TARGET_FILE:DataBase> /WHOLEARCHIVE:$<TARGET_FILE:Music>)

#if(WIN32)
# Offline diagnostic for TestModule_3DRender. Renders the cube off-screen at
# a sweep of yaw values and prints per-frame pixel-count / screen-bbox stats,
# so we can investigate "camera looks the other way and still sees a square"
# without needing an actual SDL display. Mirrors the same Renderer/Pipeline
# targets as ST_Render_Manager minus ImGui / SDL2main.
add_executable(ST_Render_Dump
    test_3d_dump.cpp
)
target_link_libraries(ST_Render_Dump PRIVATE
    Math Buffer Geometry Transform Camera Pipeline Renderer
)
target_include_directories(ST_Render_Dump PRIVATE src)
#endif()

if(WIN32)
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:ST_Render_Manager>
        COMMENT "Copying SDL2.dll"
        VERBATIM
    )

    # Copy Resource folder to output directory
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Resource"
            $<TARGET_FILE_DIR:ST_Render_Manager>/Resource
        COMMENT "Copying Resource folder"
        VERBATIM
    )

    # Copy SDL2_mixer.dll
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2_mixer::SDL2_mixer>
            $<TARGET_FILE_DIR:ST_Render_Manager>
        COMMENT "Copying SDL2_mixer.dll"
        VERBATIM
    )

    # Copy mpg123 DLL (MP3 decoder for SDL2_mixer)
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/vcpkg/installed/x64-windows/bin/mpg123.dll"
            $<TARGET_FILE_DIR:ST_Render_Manager>
        COMMENT "Copying mpg123.dll"
        VERBATIM
    )

    # Copy Data folder (Audio)
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Data"
            $<TARGET_FILE_DIR:ST_Render_Manager>/Data
        COMMENT "Copying Data folder"
        VERBATIM
    )
endif()