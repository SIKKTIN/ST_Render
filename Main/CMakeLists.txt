# Main/CMakeLists.txt
# 主程序和测试

# 可执行文件
add_executable(ST_Render
    test_main.cpp
    test_math.cpp
    test_buffer.cpp
    test_geometry.cpp
    test_transform.cpp
    test_camera.cpp
    test_pipeline.cpp
    test_renderer.cpp
    test_sdl_window.cpp
)

# 链接库
target_link_libraries(ST_Render PRIVATE Math Buffer Geometry Transform Camera Pipeline Renderer)

# 链接 SDL2
target_link_libraries(ST_Render PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# 复制 SDL2.dll 到输出目录 (vcpkg 会自动处理，这里仅作为备用)
if(WIN32)
    add_custom_command(TARGET ST_Render POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:ST_Render>
        COMMENT "Copying SDL2.dll"
        VERBATIM
    )
endif()

# SDL 窗口测试程序
add_executable(ST_Render_SDL
    test_sdl_main.cpp
    test_sdl_window.cpp
)

target_link_libraries(ST_Render_SDL PRIVATE Math Buffer Geometry Transform Camera Pipeline Renderer)

target_link_libraries(ST_Render_SDL PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

if(WIN32)
    add_custom_command(TARGET ST_Render_SDL POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:ST_Render_SDL>
        COMMENT "Copying SDL2.dll"
        VERBATIM
    )
endif()

# SDL 圆圈测试
add_executable(ST_Render_SDL_Circle
    test_sdl_circle.cpp
)

target_link_libraries(ST_Render_SDL_Circle PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

if(WIN32)
    add_custom_command(TARGET ST_Render_SDL_Circle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:ST_Render_SDL_Circle>
        COMMENT "Copying SDL2.dll"
        VERBATIM
    )
endif()

# ST_Render_Manager (Dear ImGui)
add_executable(ST_Render_Manager
    TestManager.cpp
    test_manager_main.cpp
    TestModule_FrameBuffer.cpp
    TestModule_Rasterizer.cpp
    TestModule_VertexShader.cpp
    TestModule_2D_Scene.cpp
    TestModule_Sprite2D.cpp
    TestModule_Texture.cpp
    TestModule_Music.cpp
    TestModule_NetworkTest.cpp
    test_math.cpp
    test_buffer.cpp
    test_geometry.cpp
    test_transform.cpp
    test_camera.cpp
    test_pipeline.cpp
    test_renderer.cpp
    test_sdl_window.cpp
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
