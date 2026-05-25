#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

// LOD 0 = full res, LOD 1 = half, LOD 2 = quarter, LOD 3 = eighth.
static constexpr int NUM_LODS = 4;

struct TerrainChunk {
    GLuint vao[NUM_LODS] = {};
    GLuint vbo[NUM_LODS] = {};
    GLuint ibo[NUM_LODS] = {};
    int    numIndices[NUM_LODS] = {};

    // World-space centre of this chunk (XZ plane)
    glm::vec2 centerXZ = {};

    float minY = 0.f, maxY = 0.f;

    void init(int originX, int originZ,
        int chunkVerts, float scale,
        float halfWorldX, float halfWorldZ,
        float seedX, float seedZ);

    void render(int lodLevel) const;

    void cleanup();
};

struct Terrain {

    void init(int gridW, int gridH, int chunkVerts, float scale);

    void render(const glm::vec3& cameraPos, const glm::mat4& projView);

    float getHeightAt(float xPos, float zPos, float scale) const;

    std::vector<TerrainChunk> chunks;

    // Store grid dimensions so render() can iterate
    int gridW = 0;
    int gridH = 0;
    int chunkVerts = 0;   // verts per edge at LOD 0
    float scale = 1.f;

    float seedX = 0.f;
    float seedZ = 0.f;

    // LOD distance thresholds (world units). Adjustable.
    // chunk gets LOD k when camera is further than lodDist[k-1].
    float lodDist[NUM_LODS] = { 600.f, 1500.f, 3000.f, 1e9f };

    glm::vec4 frustumPlanes[6] = {};

    void extractFrustumPlanes(const glm::mat4& projView);
    bool isChunkVisible(const TerrainChunk& chunk) const;
    int  chooseLOD(const TerrainChunk& chunk, const glm::vec3& cam) const;
};