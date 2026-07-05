# FucModule/Music - SDL_mixer wrapper
add_library(Music
    MusicManager.cpp
)

target_include_directories(Music PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Music PUBLIC
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    SDL2::SDL2
)
