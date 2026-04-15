#version 420
///////////////////////////////////////////////////////////////////////////////
// Input vertex attributes
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normalIn;
layout(location = 2) in vec2 texCoordIn;
layout(location = 4) in vec3 instanceOffset;
layout(location = 5) in vec3 terrainNormal;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;

///////////////////////////////////////////////////////////////////////////////
// Output to fragment shader
///////////////////////////////////////////////////////////////////////////////
out vec2 texCoord;
out vec3 viewSpaceNormal;
out vec3 viewSpacePosition;
out vec3 modelSpacePos;
out float height;

uniform bool isGrass;

void main()
{
	vec3 pos = position;
	vec3 vNormal = normalIn; // Start with the model's original normal

	if (isGrass) {
		float scale = 0.05;
		pos = position * scale;

		float baseOffset = 2.0;
		pos.y += baseOffset;

		vec3 worldUp = vec3(0.0, 1.0, 0.0);
		vec3 n = normalize(terrainNormal);
		vec3 axis = cross(worldUp, n);
		float angle = acos(clamp(dot(worldUp, n), -1.0, 1.0));

		if (length(axis) > 0.001) {
			axis = normalize(axis);
			float s = sin(angle);
			float c = cos(angle);
			float oc = 1.0 - c;
        
			mat3 rot = mat3(
				oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
				oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
				oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
			);
			
			pos = rot * pos;
			vNormal = rot * terrainNormal; // Rotate the normal so lighting matches the tilt!
		}
		pos.y += 0.5;
	}

	// Calculate actual position
	vec3 worldPos = pos + instanceOffset;
	
	// Use the scaled/positioned Y for height logic
	height = worldPos.y; 
	modelSpacePos = pos;

	gl_Position = modelViewProjectionMatrix * vec4(worldPos, 1.0);
	texCoord = texCoordIn;

	// FIX: Use vNormal (which was rotated) instead of the raw normalIn
	viewSpaceNormal = (normalMatrix * vec4(vNormal, 0.0)).xyz;
	
	viewSpacePosition = (modelViewMatrix * vec4(worldPos, 1.0)).xyz;
}