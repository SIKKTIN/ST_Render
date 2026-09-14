# src/engine/data/DataBase/CMakeLists.txt - Scene serialization
add_library(DataBase
    SceneData.cpp
)
target_include_directories(DataBase PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(DataBase PRIVATE Editor Math Buffer Geometry nlohmann_json::nlohmann_json)
