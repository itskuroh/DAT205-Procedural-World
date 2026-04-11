#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct Terrain {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ibo = 0;
    int numIndices = 0;

    void init(int width, int height, float scale);
    void render();
};