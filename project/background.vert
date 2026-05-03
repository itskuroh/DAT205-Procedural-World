#version 420
// This vertex shader simply outputs the input coordinates to the rasterizer. It only uses 2D coordinates.
layout(location = 0) in vec3 position;
out vec3 viewDir;
uniform mat4 inv_PV;
uniform vec3 camera_pos;

void main() {
    gl_Position = vec4(position, 1.0);
    
    vec4 unprojected = inv_PV * vec4(position, 1.0);
    vec3 worldPos = unprojected.xyz / unprojected.w;
    
    viewDir = worldPos - camera_pos;
}