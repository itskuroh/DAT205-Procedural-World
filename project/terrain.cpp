#include "terrain.h"
#include <glm/gtx/transform.hpp>
#include <cmath>
#include <cstdlib>
#include <algorithm>

//noise
static float hash(float n) { return glm::fract(glm::sin(n) * 43758.5453123f); }

static float noise(const glm::vec3& x)
{
    glm::vec3 p = glm::floor(x);
    glm::vec3 f = glm::fract(x);
    f = f * f * (3.0f - 2.0f * f);
    float n = p.x + p.y * 57.0f + 113.0f * p.z;
    return glm::mix(
        glm::mix(glm::mix(hash(n + 0.f), hash(n + 1.f), f.x),
            glm::mix(hash(n + 57.f), hash(n + 58.f), f.x), f.y),
        glm::mix(glm::mix(hash(n + 113.f), hash(n + 114.f), f.x),
            glm::mix(hash(n + 170.f), hash(n + 171.f), f.x), f.y), f.z);
}

static float computeHeight(float gx, float gz, float seedX, float seedZ)
{
    float freq = 0.012f;
    float base = noise(glm::vec3((gx + seedX) * freq, 0.f, (gz + seedZ) * freq));
    float centered = (base * 2.0f) - 1.0f;

    float h = 0.f;
    if (centered > 0.f) {
        h = std::pow(centered, 3.f) * 150.f;
        float detail = 1.f - std::abs(noise(glm::vec3(gx * 0.1f, 0.f, gz * 0.1f)));
        h += detail * 20.f * centered;
    }
    else {
        float depth = std::pow(std::abs(centered), 0.7f);
        h = -depth * 180.f;
    }
    return h;
}

void TerrainChunk::init(int originX, int originZ,
    int chunkVerts, float scale,
    float halfWorldX, float halfWorldZ,
    float seedX, float seedZ)
{
    float chunkWorldSize = (chunkVerts - 1) * scale;

    minY = 1e9f;
    maxY = -1e9f;

    // Build one mesh per LOD level
    for (int lod = 0; lod < NUM_LODS; ++lod) {
        // LOD 0 = 1 vertex per step, LOD 1 = 2, LOD 2 = 4, LOD 3 = 8
        int step = 1 << lod;  // 2^lod

        int quads = (chunkVerts - 1) / step;
        int verts = quads + 1;

        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
        vertices.reserve(verts * verts);
        indices.reserve(quads * quads * 6);

        // 1. Generate vertex positions
        for (int lz = 0; lz < verts; ++lz) {
            for (int lx = 0; lx < verts; ++lx) {
                // Grid index in the global vertex grid
                int gx = originX + lx * step;
                int gz = originZ + lz * step;

                float xPos = gx * scale - halfWorldX;
                float zPos = gz * scale - halfWorldZ;

                float yPos = computeHeight(static_cast<float>(gx),
                    static_cast<float>(gz),
                    seedX, seedZ);

                minY = std::min(minY, yPos);
                maxY = std::max(maxY, yPos);

                vertices.push_back({ glm::vec3(xPos, yPos, zPos), glm::vec3(0, 1, 0) });
            }
        }

        // 2. Generate indices + smooth normals
        for (int lz = 0; lz < quads; ++lz) {
            for (int lx = 0; lx < quads; ++lx) {
                uint32_t i0 = lz * verts + lx;
                uint32_t i1 = lz * verts + lx + 1;
                uint32_t i2 = (lz + 1) * verts + lx;
                uint32_t i3 = (lz + 1) * verts + lx + 1;

                indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
                indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);

                glm::vec3 v0 = vertices[i0].position;
                glm::vec3 v1 = vertices[i1].position;
                glm::vec3 v2 = vertices[i2].position;
                glm::vec3 normal = glm::normalize(glm::cross(v2 - v0, v1 - v0));

                // Accumulate (simple flat normal per quad vertex)
                vertices[i0].normal += normal;
                vertices[i1].normal += normal;
                vertices[i2].normal += normal;
                vertices[i3].normal += normal;
            }
        }

        // Normalise accumulated normals
        for (auto& v : vertices) {
            float len = glm::length(v.normal);
            if (len > 0.001f) v.normal = v.normal / len;
        }

        numIndices[lod] = static_cast<int>(indices.size());

        // 3. Upload to GPU
        glGenVertexArrays(1, &vao[lod]);
        glBindVertexArray(vao[lod]);

        glGenBuffers(1, &vbo[lod]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[lod]);
        glBufferData(GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
            (void*)(sizeof(glm::vec3)));
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &ibo[lod]);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo[lod]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(uint32_t),
            indices.data(), GL_STATIC_DRAW);

        glBindVertexArray(0);
    }
}

void TerrainChunk::render(int lodLevel) const
{
    int lod = glm::clamp(lodLevel, 0, NUM_LODS - 1);
    glBindVertexArray(vao[lod]);
    glDrawElements(GL_TRIANGLES, numIndices[lod], GL_UNSIGNED_INT, nullptr);
}

void TerrainChunk::cleanup()
{
    for (int i = 0; i < NUM_LODS; ++i) {
        if (vao[i]) { glDeleteVertexArrays(1, &vao[i]); vao[i] = 0; }
        if (vbo[i]) { glDeleteBuffers(1, &vbo[i]);      vbo[i] = 0; }
        if (ibo[i]) { glDeleteBuffers(1, &ibo[i]);      ibo[i] = 0; }
    }
}

void Terrain::init(int gridW_, int gridH_, int chunkVerts_, float scale_)
{
    gridW = gridW_;
    gridH = gridH_;
    chunkVerts = chunkVerts_;
    scale = scale_;

    seedX = static_cast<float>(rand() % 10000);
    seedZ = static_cast<float>(rand() % 10000);

    // Total vertices in the global grid
    int totalVertsX = gridW * (chunkVerts - 1) + 1;
    int totalVertsZ = gridH * (chunkVerts - 1) + 1;

    // Half-world size in world units (so the terrain is centred at origin)
    float halfWorldX = (totalVertsX - 1) * scale * 0.5f;
    float halfWorldZ = (totalVertsZ - 1) * scale * 0.5f;

    chunks.resize(gridW * gridH);

    for (int cz = 0; cz < gridH; ++cz) {
        for (int cx = 0; cx < gridW; ++cx) {
            int originX = cx * (chunkVerts - 1);
            int originZ = cz * (chunkVerts - 1);

            TerrainChunk& chunk = chunks[cz * gridW + cx];
            chunk.init(originX, originZ, chunkVerts, scale,
                halfWorldX, halfWorldZ,
                seedX, seedZ);

            float chunkWorldEdge = (chunkVerts - 1) * scale;
            chunk.centerXZ = glm::vec2(
                (originX + (chunkVerts - 1) * 0.5f) * scale - halfWorldX,
                (originZ + (chunkVerts - 1) * 0.5f) * scale - halfWorldZ
            );
        }
    }

    printf("Terrain: %d x %d chunks, %d verts/edge, %.0f world units total.\n",
        gridW, gridH, chunkVerts,
        (float)(gridW * (chunkVerts - 1)) * scale);
    printf("         Approx triangles at full LOD: %lld\n",
        (long long)gridW * gridH * (chunkVerts - 1) * (chunkVerts - 1) * 2LL);
}

void Terrain::extractFrustumPlanes(const glm::mat4& m)
{
    glm::mat4 t = glm::transpose(m);

    frustumPlanes[0] = t[3] + t[0]; // left
    frustumPlanes[1] = t[3] - t[0]; // right
    frustumPlanes[2] = t[3] + t[1]; // bottom
    frustumPlanes[3] = t[3] - t[1]; // top
    frustumPlanes[4] = t[3] + t[2]; // near
    frustumPlanes[5] = t[3] - t[2]; // far

    for (auto& p : frustumPlanes) {
        float len = glm::length(glm::vec3(p));
        if (len > 0.f) p /= len;
    }
}

bool Terrain::isChunkVisible(const TerrainChunk& chunk) const
{
    float halfEdge = (chunkVerts - 1) * scale * 0.5f;

    glm::vec3 bmin(chunk.centerXZ.x - halfEdge, chunk.minY, chunk.centerXZ.y - halfEdge);
    glm::vec3 bmax(chunk.centerXZ.x + halfEdge, chunk.maxY, chunk.centerXZ.y + halfEdge);

    for (const auto& plane : frustumPlanes) {
        glm::vec3 n(plane.x, plane.y, plane.z);
        // Positive vertex (furthest in plane normal direction)
        glm::vec3 pv(
            n.x >= 0 ? bmax.x : bmin.x,
            n.y >= 0 ? bmax.y : bmin.y,
            n.z >= 0 ? bmax.z : bmin.z
        );
        if (glm::dot(n, pv) + plane.w < 0.f)
            return false;  // fully outside this plane
    }
    return true;
}

int Terrain::chooseLOD(const TerrainChunk& chunk, const glm::vec3& cam) const
{
    float dist = glm::distance(
        glm::vec2(cam.x, cam.z),
        chunk.centerXZ
    );

    for (int lod = 0; lod < NUM_LODS - 1; ++lod) {
        if (dist < lodDist[lod]) return lod;
    }
    return NUM_LODS - 1;
}

void Terrain::render(const glm::vec3& cameraPos, const glm::mat4& projView)
{
    extractFrustumPlanes(projView);

    int drawn = 0, culled = 0;
    for (const auto& chunk : chunks) {
        if (!isChunkVisible(chunk)) { ++culled; continue; }
        int lod = chooseLOD(chunk, cameraPos);
        chunk.render(lod);
        ++drawn;
    }
    (void)drawn; (void)culled; // suppress unused warnings in release
}

float Terrain::getHeightAt(float xPos, float zPos, float scale_) const
{
    int totalVertsX = gridW * (chunkVerts - 1) + 1;
    int totalVertsZ = gridH * (chunkVerts - 1) + 1;

    float halfWorldX = (totalVertsX - 1) * scale_ * 0.5f;
    float halfWorldZ = (totalVertsZ - 1) * scale_ * 0.5f;

    float gx = (xPos + halfWorldX) / scale_;
    float gz = (zPos + halfWorldZ) / scale_;

    return computeHeight(gx, gz, seedX, seedZ);
}