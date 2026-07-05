#include <iostream>
#include "test_sdl_window.hpp"

using namespace ST;

int main(int argc, char* argv[]) {
    std::cout << "=== ST Render - SDL Window Demo ===" << std::endl;
    std::cout << "Press ESC or close the window to exit." << std::endl << std::endl;

    Test::testSDLWindow();

    return 0;
}
