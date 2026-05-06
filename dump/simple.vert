#version 420
layout(location = 0) in vec3 position;
//layout(location = 4) in vec3 instanceOffset;
uniform vec3 instanceOffset;

uniform mat4 modelViewProjectionMatrix;
uniform bool isGrass;

void main() {
    vec3 pos = position;
    if (isGrass) {
        pos = (position * 0.5) + instanceOffset;
    }
    gl_Position = modelViewProjectionMatrix * vec4(pos, 1.0);
}