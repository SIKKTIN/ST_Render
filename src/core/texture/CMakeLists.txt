# src/core/texture/CMakeLists.txt
add_library(Texture
    ST_Image.cpp
)
target_include_directories(Texture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)