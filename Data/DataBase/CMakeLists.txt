# Data/DataBase - Scene serialization

add_library(DataBase
    SceneData.cpp
)

target_include_directories(DataBase PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(DataBase PRIVATE Editor Math Buffer Geometry nlohmann_json::nlohmann_json)
