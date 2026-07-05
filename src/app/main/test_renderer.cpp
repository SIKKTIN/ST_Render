#include "test_renderer.hpp"
#include "renderer/renderer/Renderer.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/camera/Camera.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"

namespace ST {
namespace Test {

void testRenderer() {
    std::cout << "Testing Renderer..." << std::endl;

    Renderer renderer(800, 600);
    assert(renderer.getWidth() == 800);
    assert(renderer.getHeight() == 600);

    renderer.clear(Color::black());

    Uniform uni;
    uni.modelMatrix = Matrix4x4::identity();
    uni.viewMatrix = Matrix4x4::identity();
    uni.projectionMatrix = Matrix4x4::identity();
    uni.viewportMatrix = getViewportMatrix(800, 600);

    VertexShader vs(uni);
    Vertex vert0(Vector3(-0.5f, -0.5f, 0), Vector3::forward(), Vector2::zero(), Color::red());
    Vertex vert1(Vector3( 0.5f, -0.5f, 0), Vector3::forward(), Vector2::one(), Color::green());
    Vertex vert2(Vector3( 0.0f,  0.5f, 0), Vector3::forward(), Vector2(0.5f, 0), Color::blue());

    VertexOut out0 = vs.process(vert0);
    VertexOut out1 = vs.process(vert1);
    VertexOut out2 = vs.process(vert2);

    std::cout << "v0 clip pos: (" << out0.position.x << ", " << out0.position.y
              << ", " << out0.position.z << ", " << out0.position.w << ")" << std::endl;
    std::cout << "v1 clip pos: (" << out1.position.x << ", " << out1.position.y
              << ", " << out1.position.z << ", " << out1.position.w << ")" << std::endl;
    std::cout << "v2 clip pos: (" << out2.position.x << ", " << out2.position.y
              << ", " << out2.position.z << ", " << out2.position.w << ")" << std::endl;

    std::cout << "v0 in frustum: " << vs.isInFrustum(out0) << std::endl;
    std::cout << "v1 in frustum: " << vs.isInFrustum(out1) << std::endl;
    std::cout << "v2 in frustum: " << vs.isInFrustum(out2) << std::endl;

    FrameBuffer fb;
    fb.initialize(800, 600);
    DepthBuffer db;
    db.initialize(800, 600);

    Rasterizer raster;
    raster.setBuffers(&fb, &db);

    std::cout << "Direct rasterize test..." << std::endl;
    raster.rasterizeTriangle(out0, out1, out2,
        [](const VertexOut&) { return Color::white(); });

    const auto& pixels = fb.getPixels();

    int nonBlackPixels = 0;
    for (const auto& pixel : pixels) {
        if (pixel.r > 0 || pixel.g > 0 || pixel.b > 0) {
            nonBlackPixels++;
        }
    }

    std::cout << "Non-black pixels: " << nonBlackPixels << std::endl;
    assert(nonBlackPixels > 0);

    std::cout << "Renderer: PASSED (rendered " << nonBlackPixels << " pixels)" << std::endl;
}

} // namespace Test
} // namespace ST
