#include "test_pipeline.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/FragmentShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"

namespace ST {
namespace Test {

void testVertexShader() {
    std::cout << "Testing VertexShader..." << std::endl;

    Uniform uni;
    uni.modelMatrix = Matrix4x4::identity();
    uni.viewMatrix = Matrix4x4::identity();
    uni.projectionMatrix = Matrix4x4::identity();
    uni.viewportMatrix = Matrix4x4::identity();

    VertexShader vs(uni);

    Vertex vert(Vector3(0, 0, 0));
    VertexOut out = vs.process(vert);

    assert(out.position == uni.getMvpMatrix() * Vector4(vert.position, 1.0f));
    assert(out.worldPosition == (uni.getModelMatrix() * Vector4(vert.position, 1.0f)).toVector3());

    VertexOut inFrustum;
    inFrustum.position = Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    assert(vs.isInFrustum(inFrustum) == true);

    VertexOut outOfFrustum;
    outOfFrustum.position = Vector4(100, 100, 100, 1);
    assert(vs.isInFrustum(outOfFrustum) == false);

    std::cout << "VertexShader: PASSED" << std::endl;
}

void testFragmentShader() {
    std::cout << "Testing FragmentShader..." << std::endl;

    FragmentShader fs;

    Material mat;
    mat.ambient = Vector3(0.1f, 0.1f, 0.1f);
    mat.diffuse = Vector3(0.8f, 0.8f, 0.8f);
    mat.specular = Vector3(0.5f, 0.5f, 0.5f);
    mat.shininess = 32.0f;
    fs.setMaterial(mat);

    Light light = Light::directional(Vector3::forward(), Color::white(), 1.0f);
    fs.setLight(light);

    VertexOut vout;
    vout.worldPosition = Vector3(1, 1, 1);
    vout.normal = Vector3::up();
    vout.texCoord = Vector2::zero();
    vout.color = Color::white();

    Color result = fs.shade(vout);
    assert(result.r >= 0.0f && result.r <= 1.0f);
    assert(result.g >= 0.0f && result.g <= 1.0f);
    assert(result.b >= 0.0f && result.b <= 1.0f);

    fs.clearLights();
    Color ambientResult = fs.shade(vout);
    assert(ambientResult.r > 0.0f);

    std::cout << "FragmentShader: PASSED" << std::endl;
}

void testRasterizer() {
    std::cout << "Testing Rasterizer..." << std::endl;

    Rasterizer raster;
    assert(raster.m_frameBuffer == nullptr);
    assert(raster.m_depthBuffer == nullptr);
    assert(raster.m_width == 0);
    assert(raster.m_height == 0);

    FrameBuffer fb;
    fb.initialize(100, 100);
    DepthBuffer db;
    db.initialize(100, 100);

    raster.setBuffers(&fb, &db);
    assert(raster.m_frameBuffer == &fb);
    assert(raster.m_depthBuffer == &db);
    assert(raster.m_width == 100);
    assert(raster.m_height == 100);

    Vector2 a(0, 0), b(10, 0), c(5, 10), p(5, 3);
    Vector3 bary = Rasterizer::computeBarycentric(p, a, b, c);
    assert(bary.x >= 0 && bary.x <= 1);
    assert(bary.y >= 0 && bary.y <= 1);
    assert(bary.z >= 0 && bary.z <= 1);

    assert(std::abs(bary.x + bary.y + bary.z - 1.0f) < 0.001f);

    std::cout << "Rasterizer: PASSED" << std::endl;
}

void testLightAndMaterial() {
    std::cout << "Testing Light and Material..." << std::endl;

    Light dirLight = Light::directional(-Vector3::up(), Color::red(), 1.0f);
    assert(dirLight.type == Light::Type::DIRECTIONAL);
    assert(dirLight.direction == Vector3(0, -1, 0));
    assert(dirLight.color == Color::red());

    Light pointLight = Light::point(Vector3(5, 5, 5), Color::blue(), 1.0f, 0.1f);
    assert(pointLight.type == Light::Type::POINT);
    assert(pointLight.position == Vector3(5, 5, 5));
    assert(pointLight.color == Color::blue());
    assert(pointLight.attenuation == 0.1f);

    Material mat = Material::defaultMaterial();
    assert(mat.ambient == Vector3(0.1f, 0.1f, 0.1f));
    assert(mat.diffuse == Vector3(0.8f, 0.8f, 0.8f));
    assert(mat.specular == Vector3(0.5f, 0.5f, 0.5f));
    assert(mat.shininess == 32.0f);

    Material metallic = Material::metallic(Vector3(1, 0.8f, 0.6f), 0.3f);
    assert(metallic.shininess > 0.0f);

    std::cout << "Light and Material: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
