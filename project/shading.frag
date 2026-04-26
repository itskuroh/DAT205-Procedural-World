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

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;
uniform vec3 viewSpaceLightDir;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
//layout(location = 0) 
out vec4 fragmentColor;

uniform bool isGrass;

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
	vec3 n = normalize(viewSpaceNormal);

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

        // Transition: Sand -> Grass (around 5.0 height)
        float grassFactor = smoothstep(2.0, 2.0 + transition, height);
        terrainBase = mix(terrainBase, grassColor, grassFactor);

        // Transition: Grass -> Rock (around 35.0 height)
        float rockFactor = smoothstep(30.0, 30.0 + transition, height);
        terrainBase = mix(terrainBase, rockColor, rockFactor);

        // Transition: Rock -> Snow (around 85.0 height)
        float snowFactor = smoothstep(80.0, 80.0 + transition, height);
        terrainBase = mix(terrainBase, snowColor, snowFactor);

        float slope = 1.0 - normalize(modelNormal).y; 
        float slopeRockFactor = smoothstep(0.75, 0.85, slope);
        terrainBase = mix(terrainBase, rockColor, slopeRockFactor);

		if (height < -1.0) {
            terrainBase = mix(waterColor, sandColor, smoothstep(-5.0, -1.0, height));
        }
	}

    float diff = max(dot(n, -viewSpaceLightDir), 0.0);
    float ambient = isGrass ? 0.5 : 0.25; // Grass needs more ambient to look lush
    
    // glint
    vec3 reflectDir = reflect(-viewSpaceLightDir, n);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    float specIntensity = isGrass ? 0.1 : 0.3; // Rocks are shinier than grass

    vec3 finalColor = terrainBase * (diff + ambient) + (vec3(1.0) * spec * specIntensity);

    // Fog of War
    float dist = length(viewSpacePosition);
    // Adjust 100.0 (start) and 800.0 (end) to fit your map size
    float fogFactor = clamp((dist - 150.0) / 600.0, 0.0, 1.0); 
    vec3 fogColor = vec3(0.5, 0.6, 0.7); // Light blue/grey sky

    fragmentColor = vec4(mix(finalColor, fogColor, fogFactor), 1.0);
}
