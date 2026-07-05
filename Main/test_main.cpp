#include <iostream>
#include "test_math.hpp"
#include "test_buffer.hpp"
#include "test_geometry.hpp"
#include "test_transform.hpp"
#include "test_camera.hpp"
#include "test_pipeline.hpp"
#include "test_renderer.hpp"

using namespace ST;

int main() {
    std::cout << "=== Software Renderer Test Suite ===" << std::endl << std::endl;

    Test::testMathUtils();
    Test::testVector2();
    Test::testVector3();
    Test::testVector4();
    Test::testFrameBuffer();
    Test::testDepthBuffer();
    Test::testVertex();
    Test::testTriangle();
    Test::testMesh();
    Test::testMeshFactory();
    Test::testTransform();
    Test::testGetMVPMatrix();
    Test::testGetViewportMatrix();
    Test::testCamera();
    Test::testVertexShader();
    Test::testFragmentShader();
    Test::testRasterizer();
    Test::testLightAndMaterial();
    Test::testRenderer();

    std::cout << std::endl << "=== All tests PASSED ===" << std::endl;
    return 0;
}
