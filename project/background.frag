#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 6) uniform sampler2D environmentMap;
in vec2 texCoord;
in vec3 viewDir;
uniform vec3 lightPosition;
uniform mat4 inv_PV;
uniform vec3 camera_pos;
uniform float environment_multiplier;
#define PI 3.14159265359

void main()
{
	vec3 dir = normalize(viewDir);
    vec3 sunDir = lightPosition;
    
    float sunHeight = sunDir.y;
    vec3 skyBlue = vec3(0.2, 0.5, 0.8);
    vec3 spaceBlack = vec3(0.01, 0.01, 0.02);
    
    float distToSun = dot(dir, sunDir);
    float sunDisk = smoothstep(0.997, 0.999, distToSun);
    float sunGlow = pow(max(distToSun, 0.0), 50.0) * 0.5;

    // Day/Night Transition
    vec3 skyColor = mix(spaceBlack, skyBlue, smoothstep(-0.2, 0.5, sunHeight));
    
    // Sunset Glow
    float sunset = pow(1.0 - abs(sunHeight), 4.0) * smoothstep(-0.3, 0.1, sunHeight);
    skyColor = mix(skyColor, vec3(1.0, 0.4, 0.1), sunset);

    // Sun Disk
    vec3 sunColor = vec3(1.0, 0.9, 0.7); // Slightly yellowish
    vec3 finalColor = skyColor + (sunDisk * 2.0) + (sunGlow * sunColor);

    // moon
    float distToMoon = dot(dir, -sunDir);
    float moonDisk = smoothstep(0.998, 0.999, distToMoon);
    finalColor += moonDisk * vec3(0.8, 0.8, 1.0);

    fragmentColor = vec4(finalColor, 1.0);
}
