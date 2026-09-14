# Shader interfaces, script compiler and shader catalog.
add_library(Shader
    ScriptShaderProgram.cpp
    ShaderManager.cpp
    ShaderCatalog.cpp
)

target_include_directories(Shader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(Shader PUBLIC Math)
