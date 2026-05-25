#version 420
layout(location = 0) in vec3 position;
layout(location = 4) in vec3 instanceOffset;

uniform mat4 modelViewProjectionMatrix;
uniform bool isGrass;
uniform bool isCloud;

void main() {
    vec3 pos = position;
    if (isGrass) {
        pos = (position * 0.5) + instanceOffset;
    }
    else if (isCloud) {
        float cloudScale = 150.0;
        pos = (position * cloudScale) + instanceOffset;
    }
    gl_Position = modelViewProjectionMatrix * vec4(pos, 1.0);
}