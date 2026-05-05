#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;
//layout(binding = 10) uniform sampler2D grassTex;
//layout(binding = 11) uniform sampler2D sandTex;
//layout(binding = 12) uniform sampler2D rockTex;
in float height;
in vec3 modelSpacePos;
in vec3 modelNormal;
in vec3 worldSpacePos;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform vec3 viewSpaceLightDir;
uniform vec3 lightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
//layout(location = 0) 
out vec4 fragmentColor;

uniform bool isGrass;
uniform sampler2DShadow shadowMap;
in vec4 vFragPosLightSpace;

float calculateShadow(vec3 n, vec3 worldLightDir) {
    vec3 projCoords = vFragPosLightSpace.xyz / vFragPosLightSpace.w;
    
    // transform from [-1, 1] to [0, 1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    if(projCoords.z > 1.0 || projCoords.x > 1.0 || projCoords.x < 0.0 || projCoords.y > 1.0 || projCoords.y < 0.0) {
        return 0.0; 
    }
    // returns 1.0 if not in shadow, 0.0 if in shadow
    float bias = max(0.005 * (1.0 - dot(n, worldLightDir)), 0.0005);
    projCoords.z -= bias;
    
    return 1.0 - texture(shadowMap, projCoords);
}

vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	return vec3(material_color);
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	return vec3(0.0);
}

void main()
{
	float visibility = 1.0;
	float attenuation = 1.0;

	vec3 wo = -normalize(viewSpacePosition);
	//vec3 n = normalize(viewSpaceNormal);
    vec3 n = normalize(mat3(viewInverse) * viewSpaceNormal);

	// Direct illumination
	vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	///////////////////////////////////////////////////////////////////////////
	// Add emissive term. If emissive texture exists, sample this term.
	///////////////////////////////////////////////////////////////////////////
	vec3 emission_term = material_emission * material_color;
	if(has_emission_texture == 1)
	{
		emission_term = texture(emissiveMap, texCoord).xyz;
	}

	vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;

	vec2 uv = modelSpacePos.xz * 0.1; // tiles texture across terrain
	vec3 color = vec3(1.0, 0.0, 1.0); // Bright Magenta (something wrong)

    //vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Simple directional light
    vec3 viewDir = normalize(-viewSpacePosition);

	// Biome Colors
    vec3 waterColor = vec3(0.0, 0.1, 0.3);
    vec3 sandColor  = vec3(0.8, 0.7, 0.5);
    vec3 grassColor = vec3(0.15, 0.35, 0.08);
    vec3 rockColor  = vec3(0.35, 0.33, 0.30);
    vec3 snowColor  = vec3(0.95, 0.95, 1.0);
	vec3 terrainBase;

	if (isGrass) {
		vec3 darkGreen = vec3(0.02, 0.2, 0.02);
        vec3 lightGreen = vec3(0.3, 0.6, 0.1);
        
        float tipFactor = clamp(modelSpacePos.y * 5.0, 0.0, 1.0); 
        terrainBase = mix(darkGreen, lightGreen, tipFactor);

    } else {
        float transition = 6.0; // Smoothness of transitions
        
        // sand
        terrainBase = sandColor;

        // Transition: Sand -> Grass
        float grassFactor = smoothstep(2.0, 10.0 + transition, height);
        terrainBase = mix(terrainBase, grassColor, grassFactor);

        // Transition: Grass -> Rock
        float rockFactor = smoothstep(30.0, 30.0 + transition, height);
        terrainBase = mix(terrainBase, rockColor, rockFactor);

        // Transition: Rock -> Snow
        float snowFactor = smoothstep(80.0, 80.0 + transition, height);
        terrainBase = mix(terrainBase, snowColor, snowFactor);

        float slope = 1.0 - normalize(modelNormal).y; 
        float slopeRockFactor = smoothstep(0.75, 0.85, slope);
        terrainBase = mix(terrainBase, rockColor, slopeRockFactor);
	}

    if (height < 0.0) {
    // as height goes lower than 0, darken the color
    float depth = clamp(abs(height) / 150.0, 0.0, 1.0);
    
    vec3 abyssColor = vec3(0.01, 0.02, 0.1); 
    terrainBase = mix(terrainBase, abyssColor, depth);
    }

    // day/night cycle
    float sunLevel = viewSpaceLightPosition.y;
    // fades out when sun at altitude 50 to -10
    float directMask = smoothstep(-10.0, 50.0, sunLevel);
    float ambientMask = smoothstep(-300.0, 100.0, sunLevel);

    // sunset
    vec3 sunColor = vec3(1.0, 0.9, 0.8);
    vec3 sunsetColor = vec3(1.0, 0.5, 0.3);
    float sunsetFactor = clamp(1.0 - abs(viewSpaceLightPosition.y / 1000.0), 0.0, 1.0);
    vec3 currentLightColor = mix(sunColor, sunsetColor, pow(sunsetFactor, 2.0));

    // Diffuse
    //float diff = max(dot(n, -viewSpaceLightDir), 0.0) * directMask;
    vec3 worldLightDir = normalize(lightPosition - worldSpacePos);
    float diff = max(dot(n, worldLightDir), 0.0) * directMask;
    float shadow = calculateShadow(n, worldLightDir);
    
    // Ambient: Darker at night, lush during day
    float nightAmbient = 0.1;   //higher = brighter at night
    float dayAmbient = isGrass ? 0.5 : 0.25;
    float ambient = mix(nightAmbient, dayAmbient, ambientMask);
    
    float ambientFactor = smoothstep(-400.0, 100.0, viewSpaceLightPosition.y);
    float currentAmbient = mix(nightAmbient, dayAmbient, ambientFactor);

    // Night should be slightly blue/cool, Day should be neutral/warm
    vec3 nightTint = vec3(0.05, 0.07, 0.15);
    vec3 dayLightColor = mix(vec3(1.0, 0.5, 0.2), vec3(1.0, 0.9, 0.8), ambientFactor);

    // Combine
    vec3 diffuseTerm = (terrainBase * dayLightColor * diff) * (1.0 - shadow);
    vec3 ambientTerm = terrainBase * currentAmbient + nightTint * (1.0 - ambientFactor);
    
    //vec3 finalColor = (terrainBase * dayLightColor * diff) + ambientTerm;
    vec3 finalColor = diffuseTerm + ambientTerm;

    // fog
    float dist = length(viewSpacePosition);
    float fogFactor = clamp((dist - 300.0) / 800.0, 0.0, 1.0); 
    
    vec3 dayFog = vec3(0.5, 0.6, 0.7);
    vec3 nightFog = vec3(0.01, 0.02, 0.05);
    vec3 currentFog = mix(nightFog, dayFog, ambientMask);

    fragmentColor = vec4(mix(finalColor, currentFog, fogFactor), 1.0);
}
