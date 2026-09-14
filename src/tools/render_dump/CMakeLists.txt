# Offline 3D renderer diagnostic.
add_executable(ST_Render_Dump
    test_3d_dump.cpp
)

target_include_directories(ST_Render_Dump PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ST_Render_Dump PRIVATE
    Math Buffer Geometry Transform Camera Pipeline Renderer
)
