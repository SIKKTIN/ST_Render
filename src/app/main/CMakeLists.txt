# src/app/main/CMakeLists.txt
# 主程序和测试管理器（ImGui）

# ST_Render_Manager (Dear ImGui)
# 主入口：test_manager_main.cpp 负责启动 ImGui 框架
add_executable(ST_Render_Manager
    test_manager_main.cpp
    AppControlBridge.cpp
    HierarchyPanel.cpp
    TestModule_FrameBuffer.cpp
    TestModule_Rasterizer.cpp
    TestModule_3DRender.cpp
    TestModule_3DRender_Assets.cpp
    TestModule_Texture.cpp
    TestModule_Shader.cpp
    ../module/review/TestModule_ReviewMath.cpp
    ../module/review/TestModule_ReviewRasterizer.cpp
    imgui_impl_sdl2.cpp
    imgui_impl_sdlrenderer2.cpp
)

target_include_directories(ST_Render_Manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ST_Render_Manager PRIVATE
    Math Buffer Geometry Asset Transform Camera Pipeline Renderer Editor DataBase
    nlohmann_json::nlohmann_json
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    imgui::imgui
)

target_link_options(ST_Render_Manager PRIVATE /OPT:NOREF /WHOLEARCHIVE:$<TARGET_FILE:Editor> /WHOLEARCHIVE:$<TARGET_FILE:DataBase>)

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

    # Keep all runtime assets available beside the executable.
    add_custom_command(TARGET ST_Render_Manager POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Data"
            $<TARGET_FILE_DIR:ST_Render_Manager>/Data
        COMMENT "Copying runtime data"
        VERBATIM
    )
endif()
