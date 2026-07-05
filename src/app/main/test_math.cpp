#include "test_math.hpp"
#include "core/math/MathUtils.hpp"
#include "core/math/Vector2.hpp"
#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"

namespace ST {
namespace Test {

void testMathUtils() {
    std::cout << "Testing MathUtils..." << std::endl;

    assert(std::abs(lerp(0.0f, 10.0f, 0.5f) - 5.0f) < 0.0001f);
    assert(clamp(15.0f, 0.0f, 10.0f) == 10.0f);
    assert(clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
    assert(clamp(5.0f, 0.0f, 10.0f) == 5.0f);
    assert(isZero(0.0000001f));
    assert(!isZero(0.001f));
    assert(isEqual(0.1f + 0.2f, 0.3f));
    assert(!isEqual(1.0f, 2.0f));

    std::cout << "MathUtils: PASSED" << std::endl;
}

void testVector2() {
    std::cout << "Testing Vector2..." << std::endl;

    Vector2 a(1.0f, 2.0f);
    Vector2 b(3.0f, 4.0f);

    Vector2 c = a + b;
    assert(c.x == 4.0f && c.y == 6.0f);

    Vector2 d = a - b;
    assert(d.x == -2.0f && d.y == -2.0f);

    Vector2 e = a * 2.0f;
    assert(e.x == 2.0f && e.y == 4.0f);

    float dotResult = a.dot(b);
    assert(dotResult == 11.0f);

    assert(a.length() > 2.0f && a.length() < 3.0f);
    assert(a.lengthSquared() == 5.0f);

    Vector2 f = a.normalized();
    float lenF = f.length();
    assert(lenF > 0.99f && lenF < 1.01f);

    Vector2 g(5.0f, 0.0f);
    g.normalize();
    assert(g.x == 1.0f && g.y == 0.0f);

    Vector2 h = Vector2::lerp(Vector2(0.0f, 0.0f), Vector2(10.0f, 10.0f), 0.5f);
    assert(h.x == 5.0f && h.y == 5.0f);

    Vector2 zero = Vector2::zero();
    assert(zero.x == 0.0f && zero.y == 0.0f);

    Vector2 one = Vector2::one();
    assert(one.x == 1.0f && one.y == 1.0f);

    Vector2 right = Vector2::right();
    assert(right.x == 1.0f && right.y == 0.0f);

    Vector2 up = Vector2::up();
    assert(up.x == 0.0f && up.y == 1.0f);

    std::cout << "Vector2: PASSED" << std::endl;
}

void testVector3() {
    std::cout << "Testing Vector3..." << std::endl;

    Vector3 a(1.0f, 2.0f, 3.0f);
    Vector3 b(4.0f, 5.0f, 6.0f);

    Vector3 c = a + b;
    assert(c.x == 5.0f && c.y == 7.0f && c.z == 9.0f);

    Vector3 d = a - b;
    assert(d.x == -3.0f && d.y == -3.0f && d.z == -3.0f);

    Vector3 e = a * 2.0f;
    assert(e.x == 2.0f && e.y == 4.0f && e.z == 6.0f);

    float dotResult = a.dot(b);
    assert(dotResult == 32.0f);

    Vector3 crossResult = a.cross(b);
    assert(crossResult.x == -3.0f && crossResult.y == 6.0f && crossResult.z == -3.0f);

    assert(a.length() > 3.0f && a.length() < 4.0f);
    assert(a.lengthSquared() == 14.0f);

    Vector3 f = a.normalized();
    float lenF = f.length();
    assert(lenF > 0.99f && lenF < 1.01f);

    Vector3 g(5.0f, 0.0f, 0.0f);
    g /= 5.0f;
    assert(g.x == 1.0f && g.y == 0.0f && g.z == 0.0f);

    Vector3 zero = Vector3::zero();
    assert(zero.x == 0.0f && zero.y == 0.0f && zero.z == 0.0f);

    Vector3 up = Vector3::up();
    assert(up.x == 0.0f && up.y == 1.0f && up.z == 0.0f);

    Vector3 right = Vector3::right();
    assert(right.x == 1.0f && right.y == 0.0f && right.z == 0.0f);

    Vector3 forward = Vector3::forward();
    assert(forward.x == 0.0f && forward.y == 0.0f && forward.z == 1.0f);

    std::cout << "Vector3: PASSED" << std::endl;
}

void testVector4() {
    std::cout << "Testing Vector4..." << std::endl;

    Vector4 a(1.0f, 2.0f, 3.0f, 4.0f);
    Vector4 b(5.0f, 6.0f, 7.0f, 8.0f);

    Vector4 c = a + b;
    assert(c.x == 6.0f && c.y == 8.0f && c.z == 10.0f && c.w == 12.0f);

    Vector4 d = a - b;
    assert(d.x == -4.0f && d.y == -4.0f && d.z == -4.0f && d.w == -4.0f);

    Vector4 e = a * 2.0f;
    assert(e.x == 2.0f && e.y == 4.0f && e.z == 6.0f && e.w == 8.0f);

    float dotResult = a.dot(b);
    assert(dotResult == 70.0f);

    Vector3 vec3 = a.toVector3();
    assert(vec3.x == 1.0f && vec3.y == 2.0f && vec3.z == 3.0f);

    Vector4 perspective(2.0f, 4.0f, 6.0f, 2.0f);
    Vector3 projected = perspective.toVector3Perspective();
    assert(projected.x == 1.0f && projected.y == 2.0f && projected.z == 3.0f);

    Vector4 zero = Vector4::zero();
    assert(zero.x == 0.0f && zero.y == 0.0f && zero.z == 0.0f && zero.w == 0.0f);

    Vector4 one = Vector4::one();
    assert(one.x == 1.0f && one.y == 1.0f && one.z == 1.0f && one.w == 1.0f);

    std::cout << "Vector4: PASSED" << std::endl;
}

} // namespace Test
} // namespace ST
