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

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

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

	vec3 norm = normalize(viewSpaceNormal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0)); // Directional light
	vec3 viewDir = normalize(-viewSpacePosition); // Position from shading.vert

    float diff = max(dot(norm, lightDir), 0.25);      // Ambient + Diffuse
	if (isGrass) diff = 1.0;

	vec3 reflectDir = reflect(-lightDir, n);	//surface glint
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0); // 32 is shininess

	float ambient = isGrass ? 0.6 : 0.2;

	if (isGrass) {
		vec3 green = vec3(0.05, 0.4, 0.05);
        float tipFactor = clamp(modelSpacePos.y * 0.5, 0.0, 1.0); 
		color = mix(green, vec3(0.2, 0.6, 0.1), tipFactor);
		diff = max(diff, 0.3);

    } else if (height < 0.0) {
        color = vec3(0.0, 0.2, 0.5); // Deep Blue Water
    } else if (height < 5.0) {
        color = vec3(0.8, 0.7, 0.5); // Sand/Beach
    } else if (height < 25.0) {
        color = vec3(0.2, 0.4, 0.1); // Meadow/Grassland color for terrain
    } else if (height < 70.0) {
        color = vec3(0.4, 0.4, 0.4); // Grey Rock
    } else {
        color = vec3(0.9, 0.9, 1.0); // Snow
    }

    vec3 finalColor = color * (diff + ambient) + (vec3(1.0) * spec * 0.5);

	if (!isGrass){
		finalColor += (vec3(1.0) * spec * 0.3);
		}

	fragmentColor = vec4(finalColor*diff, 1.0);

	// fog of war
	float dist = length(viewSpacePosition);
	float fogFactor = clamp((dist - 100.0) / 800.0, 0.0, 1.0); // Start fog at 100 units

	vec3 fogColor = vec3(0.5, 0.6, 0.7); // Match this to your background sky/ocean
	fragmentColor.rgb = mix(fragmentColor.rgb, fogColor, fogFactor);
	return;
}
