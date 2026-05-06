#version 420
layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texCoordIn;

out vec2 texCoord;
out vec3 worldPos;
out float waveHeight; // Pass this to fragment to help with coloring

uniform mat4 modelViewProjectionMatrix;
uniform float currentTime;

void main() 
{
    texCoord = texCoordIn;
    vec3 pos = position;

    float wave1 = sin(pos.x * 0.05 + currentTime * 1.5) * 2.0;
    float wave2 = sin((pos.x + pos.z) * 0.07 - currentTime * 1.5) * 1.8;
    float wave3 = cos(pos.z * 0.15 + currentTime * 2.5) * 0.6;

    pos.y += (wave1 + wave2 + wave3);
    
    worldPos = pos;
    waveHeight = (wave1 + wave2 + wave3);

    gl_Position = modelViewProjectionMatrix * vec4(pos, 1.0);
}