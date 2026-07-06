// Offline diagnostic for the 3D cube renderer. Reproduces TestModule_3DRender's
// pipeline (mesh + VS + Rasterizer + back-face cull + trivial-reject) and
// runs a sweep of yaw values to surface render issues -- e.g. "camera looks
// the other way and still sees a square", or "WASD feels inverted".
//
// Output:
//   - per-yaw aggregate stats on stdout (cube pixel count, screen bbox,
//     screen center of mass)
//   - PPM image of the first frame written to --out (default build/dump.ppm)
//
// Compile is wired through src/app/main/CMakeLists.txt; the dump target links
// the same libraries as ST_Render_Manager minus ImGui / SDL2 main.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "renderer/buffer/FrameBuffer.hpp"
#include "renderer/buffer/DepthBuffer.hpp"
#include "renderer/pipeline/VertexShader.hpp"
#include "renderer/pipeline/Rasterizer.hpp"
#include "renderer/geometry/Mesh.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/math/Vector3.hpp"
#include "core/math/Vector4.hpp"

// ---------------------------------------------------------------------------
// CLI helpers
// ---------------------------------------------------------------------------
namespace {
bool argFlag(const char* needle, int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == needle) return true;
    return false;
}
std::string argValue(const char* needle, int argc, char** argv,
                     const std::string& def = "") {
    std::string prefix = std::string(needle) + "=";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind(prefix, 0) == 0) return a.substr(prefix.size());
    }
    return def;
}
float argFloat(const char* needle, int argc, char** argv, float def) {
    std::string v = argValue(needle, argc, argv);
    return v.empty() ? def : std::stof(v);
}
int argInt(const char* needle, int argc, char** argv, int def) {
    std::string v = argValue(needle, argc, argv);
    return v.empty() ? def : std::stoi(v);
}

// CCW from outside (right-hand: normal points outward):
//   -Z face (z = -h) -- vertices v0 v3 v2 v1
//   +Z face (z = +h) -- vertices v4 v5 v6 v7
//   -X face (x = -h) -- vertices v0 v4 v7 v3
//   +X face (x = +h) -- vertices v1 v2 v6 v5
//   -Y face (y = -h) -- vertices v0 v1 v5 v4
//   +Y face (y = +h) -- vertices v3 v7 v6 v2
ST::Mesh makeCube(float size) {
    ST::Mesh mesh;
    float h = size / 2.0f;
    mesh.addVertex(ST::Vertex(ST::Vector3(-h, -h, -h))); // v0
    mesh.addVertex(ST::Vertex(ST::Vector3( h, -h, -h))); // v1
    mesh.addVertex(ST::Vertex(ST::Vector3( h,  h, -h))); // v2
    mesh.addVertex(ST::Vertex(ST::Vector3(-h,  h, -h))); // v3
    mesh.addVertex(ST::Vertex(ST::Vector3(-h, -h,  h))); // v4
    mesh.addVertex(ST::Vertex(ST::Vector3( h, -h,  h))); // v5
    mesh.addVertex(ST::Vertex(ST::Vector3( h,  h,  h))); // v6
    mesh.addVertex(ST::Vertex(ST::Vector3(-h,  h,  h))); // v7

    int faces[12][3] = {
        {0, 3, 2}, {0, 2, 1},   // -Z
        {4, 5, 6}, {4, 6, 7},   // +Z
        {0, 4, 7}, {0, 7, 3},   // -X
        {1, 2, 6}, {1, 6, 5},   // +X
        {0, 1, 5}, {0, 5, 4},   // -Y
        {3, 7, 6}, {3, 6, 2}    // +Y
    };

    for (int i = 0; i < 12; ++i) mesh.addTriangle(faces[i][0], faces[i][1], faces[i][2]);
    return mesh;
}
} // namespace

// ---------------------------------------------------------------------------
// Re-implementation of TestModule_3DRender::drawMesh, kept verbatim so any
// behavior change has to be mirrored here too. If drawMesh is changed,
// update the copy here.
// ---------------------------------------------------------------------------
static void drawMeshLike(ST::FrameBuffer& fb, ST::DepthBuffer& db,
                         ST::VertexShader& vs, ST::Rasterizer& rz,
                         const ST::Mesh& mesh,
                         const ST::Matrix4x4& model,
                         const ST::Vector3& eye)
{
    rz.setBuffers(&fb, &db);
    const auto& verts = mesh.getVertices();
    const auto& idx = mesh.getIndices();

    ST::Vector3 meshCenter(0, 0, 0);
    for (const auto& v : verts) meshCenter = meshCenter + v.position;
    meshCenter = meshCenter * (1.0f / (int)verts.size());
    ST::Vector3 meshCenterWorld = (model * ST::Vector4(meshCenter, 1.0f)).toVector3();
    float boundingRadius = 0.0f;
    for (const auto& v : verts) boundingRadius = std::max(boundingRadius,
                                                         (v.position - meshCenter).length());
    bool cameraInside = (meshCenterWorld - eye).length() < boundingRadius;

    for (int i = 0; i + 2 < (int)idx.size(); i += 3) {
        ST::VertexOut v0 = vs.process(verts[idx[i + 0]]);
        ST::VertexOut v1 = vs.process(verts[idx[i + 1]]);
        ST::VertexOut v2 = vs.process(verts[idx[i + 2]]);

        bool triviallyOutside = true;
        for (const ST::VertexOut* p : {&v0, &v1, &v2}) {
            float w = p->position.w;
            if (w <= 0.0f) continue;
            if (p->position.x > -w && p->position.x < w &&
                p->position.y > -w && p->position.y < w &&
                p->position.z > -w && p->position.z < w) {
                triviallyOutside = false;
                break;
            }
        }
        if (triviallyOutside) continue;

        ST::Vector3 faceNormal = (v1.worldPosition - v0.worldPosition)
                                    .cross(v2.worldPosition - v0.worldPosition);
        ST::Vector3 toEye = eye - v0.worldPosition;
        float vis = faceNormal.dot(toEye);
        if (cameraInside) vis = -vis;
        if (vis <= 0.0f) continue;

        auto frag = [](const ST::VertexOut& f) { return f.color; };
        ST::VertexOut emitBuf[4]; int emitN = 0;
        ST::clipTriangleAgainstNearPlane(v0, v1, v2, emitBuf, emitN);
        if (emitN == 3) {
            rz.rasterizeTriangle(emitBuf[0], emitBuf[1], emitBuf[2], frag);
        } else if (emitN == 4) {
            rz.rasterizeTriangle(emitBuf[0], emitBuf[1], emitBuf[2], frag);
            rz.rasterizeTriangle(emitBuf[0], emitBuf[2], emitBuf[3], frag);
        }
    }
}

static void renderOne(ST::FrameBuffer& fb, ST::DepthBuffer& db,
                      ST::VertexShader& vs, ST::Rasterizer& rz,
                      const ST::Mesh& cube,
                      const ST::Vector3& eye, float yaw, float pitch,
                      int W, int H)
{
    fb.clear(ST::Color(0.08f, 0.09f, 0.12f, 1.0f));
    db.clear();

    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw),   sy = std::sin(yaw);

    ST::Vector3 forward(-cp * sy, -sp, -cp * cy);
    ST::Vector3 target = eye + forward;
    ST::Vector3 up(0, 1, 0);

    ST::Matrix4x4 view = ST::Matrix4x4::lookAt(eye, target, up);
    float aspect = (float)W / (float)H;
    ST::Matrix4x4 proj = ST::Matrix4x4::perspective(
        (float)M_PI / 3.0f, aspect, 0.1f, 100.0f);

    ST::Uniform uni;
    uni.modelMatrix = ST::Matrix4x4::identity();
    uni.viewMatrix = view;
    uni.projectionMatrix = proj;
    vs.setUniform(uni);
    (void)vs; // silenced -- setUniform stores it for process() below

    rz.setUseRawScreenCoords(false);
    drawMeshLike(fb, db, vs, rz, cube, uni.modelMatrix, eye);
}

static void dumpStats(const char* tag, int W, int H,
                      const std::vector<ST::Color>& px,
                      ST::Color clear)
{
    long long cubePx = 0;
    int minX = W, minY = H, maxX = -1, maxY = -1;
    long long sumX = 0, sumY = 0;
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const ST::Color& c = px[y * W + x];
            float d = std::fabs(c.r - clear.r)
                    + std::fabs(c.g - clear.g)
                    + std::fabs(c.b - clear.b);
            if (d > 0.01f) {
                ++cubePx;
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
                sumX += x;
                sumY += y;
            }
        }
    }
    float frac = (float)cubePx / (W * H) * 100.0f;
    float cx = cubePx ? (float)sumX / cubePx : -1.0f;
    float cy = cubePx ? (float)sumY / cubePx : -1.0f;
    std::printf("  %-30s cubePx=%7lld frac=%5.2f%% bbox=[%d,%d..%d,%d] center=(%.1f,%.1f)\n",
                tag, cubePx, frac, minX, minY, maxX, maxY, cx, cy);
}

static void dumpPPM(const std::string& path, int W, int H,
                    const std::vector<ST::Color>& px)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << path << "\n"; return; }
    f << "P6\n" << W << " " << H << "\n255\n";
    for (int i = 0; i < W * H; ++i) {
        const ST::Color& c = px[i];
        auto sat = [](float v) {
            if (v < 0.0f) return 0;
            if (v > 1.0f) return 255;
            return (int)(v * 255.0f);
        };
        char rgb[3] = { (char)sat(c.r), (char)sat(c.g), (char)sat(c.b) };
        f.write(rgb, 3);
    }
}

// Also produce a low-resolution ASCII map so we can eyeball the frame
// without an image viewer. Each cell is one of:
//   . background
//   X cube (any non-clear pixel)
//   # cube dense (cell average > 0.5 luma)
static void dumpASCII(int W, int H, const std::vector<ST::Color>& px,
                      ST::Color clear)
{
    constexpr int GROUPSIZE = 6;
    int gw = (W + GROUPSIZE - 1) / GROUPSIZE;
    int gh = (H + GROUPSIZE - 1) / GROUPSIZE;
    std::printf("\nASCII (one char per %dx%d block):\n", GROUPSIZE, GROUPSIZE);
    for (int gy = 0; gy < gh; ++gy) {
        for (int gx = 0; gx < gw; ++gx) {
            int cnt = 0, darkCnt = 0;
            for (int dy = 0; dy < GROUPSIZE; ++dy) {
                for (int dx = 0; dx < GROUPSIZE; ++dx) {
                    int x = gx * GROUPSIZE + dx;
                    int y = gy * GROUPSIZE + dy;
                    if (x >= W || y >= H) continue;
                    const ST::Color& c = px[y * W + x];
                    float d = std::fabs(c.r - clear.r) + std::fabs(c.g - clear.g) + std::fabs(c.b - clear.b);
                    if (d > 0.01f) ++cnt;
                    if (c.r + c.g + c.b > 0.6f) ++darkCnt;
                }
            }
            char ch = '.';
            if (cnt > GROUPSIZE * GROUPSIZE / 3) ch = 'X';
            if (darkCnt > GROUPSIZE * GROUPSIZE / 2) ch = '#';
            std::putchar(ch);
        }
        std::putchar('\n');
    }
}

int main(int argc, char** argv) {
    const int W = argInt("--w", argc, argv, 160);
    const int H = argInt("--h", argc, argv, 120);
    const float eyeX = argFloat("--eye-x", argc, argv, 1.6f);
    const float eyeY = argFloat("--eye-y", argc, argv, 1.0f);
    const float eyeZ = argFloat("--eye-z", argc, argv, 2.5f);
    const float yawStart = argFloat("--yaw", argc, argv, 0.0f);
    const float pitch = argFloat("--pitch", argc, argv, 0.35f);
    const bool sweep = argFlag("--sweep", argc, argv);
    const int sweepN = argInt("--sweep-n", argc, argv, 16);
    const std::string outPath = argValue("--out", argc, argv, "");

    ST::FrameBuffer fb;
    ST::DepthBuffer db;
    fb.initialize(W, H);
    db.initialize(W, H);
    ST::VertexShader vs;
    ST::Rasterizer rz;
    rz.setBuffers(&fb, &db);

    ST::Mesh cube = makeCube(1.0f);

    ST::Vector3 eye(eyeX, eyeY, eyeZ);
    ST::Color clear(0.08f, 0.09f, 0.12f, 1.0f);

    std::printf("ST_Render_Dump: W=%d H=%d eye=(%.2f,%.2f,%.2f) yaw=%.3f pitch=%.3f\n",
                W, H, eyeX, eyeY, eyeZ, yawStart, pitch);

    if (sweep) {
        for (int i = 0; i < sweepN; ++i) {
            float yaw = (float)i / sweepN * 2.0f * (float)M_PI;
            renderOne(fb, db, vs, rz, cube, eye, yaw, pitch, W, H);
            char tag[64];
            std::snprintf(tag, sizeof(tag), "yaw=%.2fpi", yaw / (float)M_PI);
            dumpStats(tag, W, H, fb.getPixels(), clear);
        }
        // Also report which triangles fail back-face culling at each yaw,
        // so we can correlate "screen full of pixels" frames with the set
        // of triangles we kept.
        std::printf("\nTriangle visibility audit (camera = (%.2f,%.2f,%.2f), pitch=%.3f):\n",
                    eye.x, eye.y, eye.z, pitch);
        for (int i = 0; i < sweepN; ++i) {
            float yaw = (float)i / sweepN * 2.0f * (float)M_PI;
            float cp = std::cos(pitch), sp = std::sin(pitch);
            ST::Vector3 forward(-cp * std::sin(yaw), -sp, -cp * std::cos(yaw));
            ST::Vector3 target = eye + forward;
            ST::Matrix4x4 view = ST::Matrix4x4::lookAt(eye, target, ST::Vector3(0, 1, 0));
            ST::Matrix4x4 proj = ST::Matrix4x4::perspective((float)M_PI/3.0f, (float)W/H, 0.1f, 100.0f);

            ST::Uniform uni;
            uni.modelMatrix = ST::Matrix4x4::identity();
            uni.viewMatrix = view;
            uni.projectionMatrix = proj;
            vs.setUniform(uni);

        // meshCenter / inside test
        ST::Vector3 meshCenter(0,0,0);
        for (auto& v : cube.getVertices()) meshCenter = meshCenter + v.position;
        meshCenter = meshCenter * (1.0f / (int)cube.getVertices().size());
        float br = 0; for (auto& v : cube.getVertices()) br = std::max(br, (v.position - meshCenter).length());
        bool inside = (meshCenter - eye).length() < br;

        std::printf("  yaw=%.2fpi: ", yaw / (float)M_PI);
        for (int j = 0; j + 2 < (int)cube.getIndices().size(); j += 3) {
            ST::VertexOut v0 = vs.process(cube.getVertices()[cube.getIndices()[j + 0]]);
            ST::VertexOut v1 = vs.process(cube.getVertices()[cube.getIndices()[j + 1]]);
            ST::VertexOut v2 = vs.process(cube.getVertices()[cube.getIndices()[j + 2]]);
            ST::Vector3 fn = (v1.worldPosition - v0.worldPosition)
                                .cross(v2.worldPosition - v0.worldPosition);
            ST::Vector3 toEye = eye - v0.worldPosition;
            float vis = fn.dot(toEye);
            if (inside) vis = -vis;

            // Near-plane clip
            ST::VertexOut emitBuf[4]; int emitN = 0;
            ST::clipTriangleAgainstNearPlane(v0, v1, v2, emitBuf, emitN);
            char tag = (emitN == 0 ? 'X' : (emitN == 3 ? 'K' : 'Q'));
            if (j == 0) {
                std::printf("[w0=%.3f w1=%.3f w2=%.3f]",
                            v0.position.w, v1.position.w, v2.position.w);
            }
            bool cullKeep = (vis > 0);
            std::printf("%d%c%c", j / 3, cullKeep ? 'K' : 'c', tag);
        }
        std::printf("\n");
    }
    } else {
        renderOne(fb, db, vs, rz, cube, eye, yawStart, pitch, W, H);
        char tag[64];
        std::snprintf(tag, sizeof(tag), "yaw=%.3f", yawStart);
        dumpStats(tag, W, H, fb.getPixels(), clear);

        // Print all 12 face normals to see whether cube is wound CCW or CW.
        std::printf("\nReference face normals (mesh-local, model=identity):\n");
        for (int j = 0; j + 2 < (int)cube.getIndices().size(); j += 3) {
            ST::Vertex v0 = cube.getVertices()[cube.getIndices()[j + 0]];
            ST::Vertex v1 = cube.getVertices()[cube.getIndices()[j + 1]];
            ST::Vertex v2 = cube.getVertices()[cube.getIndices()[j + 2]];
            ST::Vector3 fn = (v1.position - v0.position)
                                .cross(v2.position - v0.position);
            std::printf("  tri %2d (v%d,v%d,v%d)  fn = (%.2f,%.2f,%.2f)\n",
                       j / 3,
                       cube.getIndices()[j + 0],
                       cube.getIndices()[j + 1],
                       cube.getIndices()[j + 2],
                       fn.x, fn.y, fn.z);
        }

        dumpASCII(W, H, fb.getPixels(), clear);
        if (!outPath.empty()) {
            dumpPPM(outPath, W, H, fb.getPixels());
            std::printf("PPM written to %s\n", outPath.c_str());
        }
    }
    return 0;
}
