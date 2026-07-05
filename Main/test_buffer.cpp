#include "test_buffer.hpp"
#include "FrameBuffer.hpp"
#include "DepthBuffer.hpp"

namespace ST {
namespace Test {

void testFrameBuffer() {
    std::cout << "Testing FrameBuffer..." << std::endl;

    FrameBuffer fb;
    fb.initialize(4, 4);

    assert(fb.getWidth() == 4);
    assert(fb.getHeight() == 4);

    fb.clear(Color::black());
    Color pixel1 = fb.getPixel(0, 0);
    assert(pixel1 == Color::black());

    fb.setPixel(1, 2, Color::red());
    Color pixel2 = fb.getPixel(1, 2);
    assert(pixel2 == Color::red());

    fb.setPixel(-1, 0, Color::white());
    Color pixel3 = fb.getPixel(-1, 0);
    assert(pixel3 == Color::black());

    fb.setPixel(5, 5, Color::blue());
    Color pixel4 = fb.getPixel(5, 5);
    assert(pixel4 == Color::black());

    fb.clear(Color::green());
    Color pixel5 = fb.getPixel(3, 3);
    assert(pixel5 == Color::green());

    std::cout << "FrameBuffer: PASSED" << std::endl;
}

void testDepthBuffer() {
    std::cout << "Testing DepthBuffer..." << std::endl;

    DepthBuffer db;
    db.initialize(4, 4);

    assert(db.getWidth() == 4);
    assert(db.getHeight() == 4);

    db.clear(1.0f);
    assert(db.getDepth(0, 0) == 1.0f);

    assert(db.testAndSet(1, 1, 0.5f) == true);
    assert(db.getDepth(1, 1) == 0.5f);

    assert(db.testAndSet(1, 1, 0.3f) == true);
    assert(db.getDepth(1, 1) == 0.3f);

    assert(db.testAndSet(1, 1, 0.8f) == false);
    assert(db.getDepth(1, 1) == 0.3f);

    assert(db.testAndSet(-1, 0, 0.1f) == false);
    assert(db.testAndSet(0, -1, 0.1f) == false);
    assert(db.testAndSet(5, 0, 0.1f) == false);
    assert(db.testAndSet(0, 5, 0.1f) == false);

    std::cout << "DepthBuffer: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
