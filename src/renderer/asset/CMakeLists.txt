# Asset loading and repository model catalog.
add_library(Asset
    ObjModelLoader.cpp
    ModelCatalog.cpp
)
target_include_directories(Asset PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Asset PUBLIC Geometry Math)
