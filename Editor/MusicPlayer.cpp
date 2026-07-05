#include "MusicPlayer.hpp"

ST::MusicPlayer::MusicPlayer()
    : GameObject("MusicPlayer") {
    shapeType = ShapeType::None;
}

ST::MusicPlayer::MusicPlayer(const std::string& name)
    : GameObject(name) {
    shapeType = ShapeType::None;
}

ST::MusicPlayer::~MusicPlayer() = default;
