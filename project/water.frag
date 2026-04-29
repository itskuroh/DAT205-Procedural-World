#version 420

in vec2 texCoord;
in vec3 worldPos;
in float waveHeight; 
out vec4 fragmentColor;

uniform float currentTime;
uniform vec3 waterColor;
uniform vec3 cameraPosition;

void main() 
{
    vec3 viewDir = normalize(cameraPosition - worldPos);
    vec3 normal = normalize(vec3(-waveHeight * 0.2, 1.0, -waveHeight * 0.1));
    float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 5.0);

    // waveHeight is high, blend in some white
    float foam = smoothstep(3.5, 5.0, waveHeight);
    vec3 whitecapColor = vec3(0.9, 0.95, 1.0);

    vec3 shallowColor = waterColor + vec3(0.05, 0.1, 0.1);
    vec3 baseColor = mix(waterColor, shallowColor, clamp(waveHeight * 0.2, 0.0, 1.0));

    // white foam at wave peak
    vec3 finalColor = mix(baseColor, whitecapColor, foam);
    
    // sky shine
    vec3 skyColor = vec3(0.7, 0.8, 1.0);
    finalColor = mix(finalColor, skyColor, fresnel * 0.5);

    // transparency
    fragmentColor = vec4(finalColor, 0.75);
}