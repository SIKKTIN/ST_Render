# Asset loading and repository model catalog.
add_library(Asset
    ObjModelLoader.cpp
    AssimpModelLoader.cpp
    ModelLoader.cpp
    ModelCatalog.cpp
    TextureCatalog.cpp
)
target_include_directories(Asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
find_package(assimp CONFIG REQUIRED)
target_link_libraries(Asset PUBLIC Geometry Math assimp::assimp)
