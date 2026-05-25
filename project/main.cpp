
#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"
#include "terrain.h"
#include <vector>
#include <ctime>


///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;
//float terrainSize = 3000.0f;		//change as needed
int waterVertexCount;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

int grassCount = 100;
const int maxGrassCount = 10000;
float daySpeed = 0.5f;
float dayAngle = 0.0f;

const int   TERRAIN_GRID_W = 105;      // chunks along X
const int   TERRAIN_GRID_H = 105;      // chunks along Z
const int   TERRAIN_CHUNK_V = 33;      // vertices per chunk edge (must be 2^n+1)
const float terrainScale = 1.5f;    // world units per vertex step (was hardcoded 1.5f before)
float terrainSize = (TERRAIN_GRID_W * (TERRAIN_CHUNK_V - 1) * terrainScale) * 0.5f;

//cloud globals
std::vector<labhelper::Model*> cloudModels;
std::vector<int> cloudModelIndices;
std::vector<glm::vec3> cloudPositions;
int numClouds = 20; // tune this
GLuint cloudPosBuffer = 0;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;

GLuint shadowMapFBO;
GLuint shadowMapFBODepth;
const int shadowMapSize = 2048; // Resolution of shadow map

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap;
const std::string envmap_base_name = "001";

// water
GLuint waterVAO, waterVBO;
float waterHeight = -10.0f; // slider control
GLuint waterShaderProgram;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;

///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-250.0f, 250.0f, 450.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 300.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////

mat4 roomModelMatrix;
labhelper::Model* grassModel = nullptr;

Terrain myTerrain;

struct SimpleMesh {
	GLuint vao = 0;
	int vertexCount = 0;
};

SimpleMesh grassMesh;
std::vector<glm::vec3> grassPositions;
GLuint grassInstanceBuffer = 0;

std::vector<glm::vec3> grassNormals; // To store the tilt
GLuint grassNormalBuffer = 0;

void generateGrass();
void loadShaders(bool is_reload);

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag", is_reload);
	if (shader != 0)
	{
		simpleShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag", is_reload);
	if (shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if (shader != 0)
	{
		shaderProgram = shader;
	}
}

void initWater() {
	std::vector<vec3> vertices;
	std::vector<vec2> texCoords;
	std::vector<uint32_t> indices;

	int res = 200; // Resolution: 200x200 grid
	float size = 5000.0f;

	for (int z = 0; z <= res; z++) {
		for (int x = 0; x <= res; x++) {
			// Position
			float xPos = ((float)x / res - 0.5f) * size;
			float zPos = ((float)z / res - 0.5f) * size;
			vertices.push_back(vec3(xPos, 0.0f, zPos));

			// repeating waves
			texCoords.push_back(vec2((float)x / res * 50.0f, (float)z / res * 50.0f));
		}
	}

	for (int z = 0; z < res; z++) {
		for (int x = 0; x < res; x++) {
			int i0 = z * (res + 1) + x;
			int i1 = i0 + 1;
			int i2 = (z + 1) * (res + 1) + x;
			int i3 = i2 + 1;
			indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
			indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
		}
	}

	waterVertexCount = (int)indices.size();

	glGenVertexArrays(1, &waterVAO);
	glBindVertexArray(waterVAO);

	GLuint vboPos, vboTex, ibo;
	glGenBuffers(1, &vboPos);
	glBindBuffer(GL_ARRAY_BUFFER, vboPos);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vec3), vertices.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &vboTex);
	glBindBuffer(GL_ARRAY_BUFFER, vboTex);
	glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(vec2), texCoords.data(), GL_STATIC_DRAW);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	glGenBuffers(1, &ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

	glBindVertexArray(0);
}

//shadow
void initShadowMap() {
	glGenFramebuffers(1, &shadowMapFBO);
	glGenTextures(1, &shadowMapFBODepth);
	glBindTexture(GL_TEXTURE_2D, shadowMapFBODepth);

	// Depth texture setup
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, shadowMapSize, shadowMapSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Hardware shadow comparison (important for sampler2DShadow)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	// Clamp to border to prevent "shadow stretching" outside the light frustum
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMapFBODepth, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();
	srand(static_cast<unsigned int>(time(NULL)));
	myTerrain.init(TERRAIN_GRID_W, TERRAIN_GRID_H, TERRAIN_CHUNK_V, terrainScale);
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	grassModel = labhelper::loadModelFromOBJ("../scenes/grass3.obj");
	if (grassModel == nullptr) {
		printf("ERROR: grass obj file not found!\n");
	}
	else {
		printf("SUCCESS: Loaded %zu meshes for grass.\n", grassModel->m_meshes.size());
	}
	generateGrass();

	// Attach the instance buffer to the Model's VAO
	glBindVertexArray(grassModel->m_vaob);

	glBindBuffer(GL_ARRAY_BUFFER, grassInstanceBuffer);
	glEnableVertexAttribArray(4); // Location 4 in your shading.vert
	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

	// Crucial: tell OpenGL this is per-instance data
	glVertexAttribDivisor(4, 1);

	// for normal of grass texture
	glBindBuffer(GL_ARRAY_BUFFER, grassNormalBuffer);
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glVertexAttribDivisor(5, 1);

	glBindVertexArray(0);

	//load clouds
	cloudModels.push_back(labhelper::loadModelFromOBJ("../scenes/CloudMedium.obj"));
	cloudModels.push_back(labhelper::loadModelFromOBJ("../scenes/CloudSmall.obj"));
	cloudModels.push_back(labhelper::loadModelFromOBJ("../scenes/CloudSmall2.obj"));
	srand(42); // fixed seed so clouds are same every run
	float halfWorld = terrainSize; // reuse your existing terrainSize
	cloudModelIndices.clear();
	for (int i = 0; i < numClouds; i++) {
		float x = labhelper::uniform_randf(-halfWorld, halfWorld);
		float z = labhelper::uniform_randf(-halfWorld, halfWorld);
		float y = labhelper::uniform_randf(600.0f, 900.0f);
		cloudPositions.push_back(glm::vec3(x, y, z));
		cloudModelIndices.push_back(rand() % cloudModels.size()); // random model
	}

	// Upload positions to GPU as instance buffer
	glGenBuffers(1, &cloudPosBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
	glBufferData(GL_ARRAY_BUFFER,
		cloudPositions.size() * sizeof(glm::vec3),
		cloudPositions.data(), GL_DYNAMIC_DRAW); // DYNAMIC so we can update each frame

	// Attach to cloud VAO at location 4 (instance offset, same as grass)
	for (auto* model : cloudModels) {
		glBindVertexArray(model->m_vaob);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
		glVertexAttribDivisor(4, 1);
		glBindVertexArray(0);
	}

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	waterShaderProgram = labhelper::loadShaderProgram("../project/water.vert", "../project/water.frag");
	initWater();

	initShadowMap();

	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
}

void debugDrawLight(const glm::mat4& viewMatrix,
	const glm::mat4& projectionMatrix,
	const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	//labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	//                          projectionMatrix * viewMatrix * modelMatrix);
	//labhelper::render(sphereModel);
}

void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::setUniformSlow(backgroundProgram, "lightPosition", normalize(lightPosition));
	labhelper::drawFullScreenQuad();
}

glm::vec3 getTerrainNormal(float x, float z, float terrainScale) {
	float delta = 1.0f;
	float hL = myTerrain.getHeightAt(x - delta, z, terrainScale);
	float hR = myTerrain.getHeightAt(x + delta, z, terrainScale);
	float hD = myTerrain.getHeightAt(x, z - delta, terrainScale);
	float hU = myTerrain.getHeightAt(x, z + delta, terrainScale);

	// This calculates the slope in both directions
	glm::vec3 normal;
	normal.x = hL - hR;
	normal.y = 2.0f * delta;
	normal.z = hD - hU;

	return glm::normalize(glm::vec3(hR - hL, 2.0f * delta, hU - hD));
}

void generateGrass() {
	grassPositions.clear();
	grassNormals.clear();
	float halfSize = terrainSize;

	for (int i = 0; i < 3000; i++) {
		float x = labhelper::uniform_randf(-halfSize, halfSize);
		float z = labhelper::uniform_randf(-halfSize, halfSize);

		// Call our new height function
		float h = myTerrain.getHeightAt(x, z, terrainScale);

		glm::vec3 normal = getTerrainNormal(x, z, terrainScale);
		float slope = 1.0f - normal.y;

		// Place grass only on the plains (above sand, below rock)
		if (h > 2.0f && h < 35.0f && slope < 0.75f) {
			grassPositions.push_back(glm::vec3(x, h, z));

			grassNormals.push_back(getTerrainNormal(x, z, terrainScale));
		}
	}

	if (grassInstanceBuffer == 0) glGenBuffers(1, &grassInstanceBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, grassInstanceBuffer);
	glBufferData(GL_ARRAY_BUFFER, grassPositions.size() * sizeof(glm::vec3), grassPositions.data(), GL_STATIC_DRAW);

	if (grassNormalBuffer == 0) glGenBuffers(1, &grassNormalBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, grassNormalBuffer);
	glBufferData(GL_ARRAY_BUFFER, grassNormals.size() * sizeof(glm::vec3), grassNormals.data(), GL_STATIC_DRAW);

	printf("Normal Buffer ID: %u\n", grassNormalBuffer); // Should NOT be 0
	// Unbind to stay clean
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram,
	const mat4& viewMatrix,
	const mat4& projectionMatrix,
	const mat4& lightViewMatrix,
	const mat4& lightProjectionMatrix,
	const vec3& vsLightDir)
{
	glUseProgram(currentShaderProgram);

	// slider variables
	labhelper::setUniformSlow(currentShaderProgram, "currentTime", currentTime);


	// Light source
	if (currentShaderProgram == shaderProgram) {
		vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
		labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
		labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
			point_light_intensity_multiplier);
		labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
		labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir", vsLightDir);
		labhelper::setUniformSlow(currentShaderProgram, "lightPosition", lightPosition);
	}

	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// terrain
	mat4 modelMatrix = mat4(1.0f);
	mat4 modelViewMatrix = viewMatrix * modelMatrix;
	mat4 normalMatrix = transpose(inverse(modelViewMatrix));

	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix", projectionMatrix * modelViewMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", modelViewMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix", normalMatrix);

	mat4 projViewMatrix = projectionMatrix * viewMatrix;
	myTerrain.render(cameraPosition, projectionMatrix * viewMatrix);

	//draw water
	if (currentShaderProgram == shaderProgram) {
		glUseProgram(waterShaderProgram);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_CULL_FACE);

		// Set the water's position based on the slider 'waterHeight'
		mat4 waterModelMatrix = translate(mat4(1.0f), vec3(0, waterHeight, 0));
		mat4 waterMVP = projectionMatrix * viewMatrix * waterModelMatrix;

		labhelper::setUniformSlow(waterShaderProgram, "modelViewProjectionMatrix", waterMVP);
		labhelper::setUniformSlow(waterShaderProgram, "currentTime", currentTime);
		labhelper::setUniformSlow(waterShaderProgram, "waterColor", vec3(0.1f, 0.4f, 0.6f));
		labhelper::setUniformSlow(waterShaderProgram, "cameraPosition", cameraPosition);

		glBindVertexArray(waterVAO);
		glDrawElements(GL_TRIANGLES, waterVertexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);

		glDisable(GL_BLEND);
		glEnable(GL_CULL_FACE);
	}

	//draw grass
	if (grassModel != nullptr && !grassPositions.empty()) {
		glUseProgram(currentShaderProgram);
		labhelper::setUniformSlow(currentShaderProgram, "isGrass", true);
		glDisable(GL_CULL_FACE);

		glBindVertexArray(grassModel->m_vaob);

		// We loop through each mesh in the OBJ and draw all instances of it
		for (const auto& mesh : grassModel->m_meshes) {
			glDrawArraysInstanced(
				GL_TRIANGLES,
				mesh.m_start_index,       // Where this part of the OBJ starts
				mesh.m_number_of_vertices, // How many vertices in this part
				(GLsizei)grassCount
			);
		}

		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
		labhelper::setUniformSlow(currentShaderProgram, "isGrass", false);
	}

	//draw clouds
	if (!cloudModels.empty() && !cloudPositions.empty()) {
		glUseProgram(currentShaderProgram);
		labhelper::setUniformSlow(currentShaderProgram, "isCloud", true);
		labhelper::setUniformSlow(currentShaderProgram, "isGrass", false);

		mat4 cloudMV = viewMatrix * mat4(1.0f);
		labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", cloudMV);
		labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
			projectionMatrix * cloudMV);
		glDisable(GL_CULL_FACE);

		// Upload ALL positions at once before drawing
		glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
		glBufferSubData(GL_ARRAY_BUFFER, 0,
			cloudPositions.size() * sizeof(glm::vec3),
			cloudPositions.data());

		// Draw each cloud with its assigned model
		for (int i = 0; i < (int)cloudPositions.size(); i++) {
			labhelper::Model* model = cloudModels[cloudModelIndices[i]];

			glBindVertexArray(model->m_vaob);
			// Point instance attribute to just this cloud's slot in the buffer
			glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
			glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE,
				sizeof(glm::vec3),
				(void*)(i * sizeof(glm::vec3)));
			for (const auto& mesh : model->m_meshes) {
				glDrawArraysInstanced(GL_TRIANGLES,
					mesh.m_start_index,
					mesh.m_number_of_vertices,
					1);
			}
		}

		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
		labhelper::setUniformSlow(currentShaderProgram, "isCloud", false);

		mat4 origMV = viewMatrix * mat4(1.0f);
		labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", origMV);
		labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
			projectionMatrix * origMV);
	}

}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s("Display");

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if (w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 5000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);
	vec3 vsLightDir = normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f)));

	float radius = 1200.0f;
	lightPosition = vec3(0.0f, sin(dayAngle) * radius, cos(dayAngle) * radius);
	vec3 sunDir = normalize(lightPosition);
	vec3 shadowCenter = vec3(cameraPosition.x, 0.0f, cameraPosition.z);

	mat4 lightViewMatrix = lookAt(shadowCenter + sunDir * 2000.0f, shadowCenter, worldUp);

	mat4 invProjView = inverse(projMatrix * viewMatrix);
	vec4 ndcCorners[8] = {
		{-1,-1,-1,1}, { 1,-1,-1,1}, {-1, 1,-1,1}, { 1, 1,-1,1},
		{-1,-1, 1,1}, { 1,-1, 1,1}, {-1, 1, 1,1}, { 1, 1, 1,1},
	};
	float lsMinX = 1e9f, lsMaxX = -1e9f;
	float lsMinY = 1e9f, lsMaxY = -1e9f;
	float lsMinZ = 1e9f, lsMaxZ = -1e9f;
	for (auto& c : ndcCorners) {
		vec4 world = invProjView * c;
		world /= world.w;
		vec4 ls = lightViewMatrix * world;
		lsMinX = min(lsMinX, ls.x); lsMaxX = max(lsMaxX, ls.x);
		lsMinY = min(lsMinY, ls.y); lsMaxY = max(lsMaxY, ls.y);
		lsMinZ = min(lsMinZ, ls.z); lsMaxZ = max(lsMaxZ, ls.z);
	}
	lsMinZ -= 2000.0f;  // was 500.0f — pull back further to catch tall casters behind camera
	lsMaxY += 400.0f;   // extend upward in light space to catch peak geometry
	lsMinY -= 400.0f;   // extend downward symmetrically

	mat4 lightProjMatrix = ortho(lsMinX, lsMaxX, lsMinY, lsMaxY, -lsMaxZ, -lsMinZ);
	mat4 lightSpaceMatrix = lightProjMatrix * lightViewMatrix;

	//render to shadow map
	glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
	glViewport(0, 0, shadowMapSize, shadowMapSize);
	glClear(GL_DEPTH_BUFFER_BIT);

	//simple shader for depth
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	drawScene(simpleShaderProgram, lightViewMatrix, lightProjMatrix, lightViewMatrix, lightProjMatrix, vsLightDir);

	if (!cloudModels.empty() && !cloudPositions.empty()) {
		glUseProgram(simpleShaderProgram);
		glDisable(GL_CULL_FACE);
		labhelper::setUniformSlow(simpleShaderProgram, "isCloud", true);
		labhelper::setUniformSlow(simpleShaderProgram, "isGrass", false);

		mat4 cloudMVP = lightProjMatrix * lightViewMatrix;
		labhelper::setUniformSlow(simpleShaderProgram, "modelViewProjectionMatrix", cloudMVP);

		for (int i = 0; i < (int)cloudPositions.size(); i++) {
			labhelper::Model* model = cloudModels[cloudModelIndices[i]];

			glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
			glBufferSubData(GL_ARRAY_BUFFER, i * sizeof(glm::vec3),
				sizeof(glm::vec3), &cloudPositions[i]);

			glBindVertexArray(model->m_vaob);
			for (const auto& mesh : model->m_meshes) {
				glDrawArraysInstanced(GL_TRIANGLES,
					mesh.m_start_index,
					mesh.m_number_of_vertices,
					1);
			}
		}

		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
		labhelper::setUniformSlow(simpleShaderProgram, "isCloud", false);
	}

	glCullFace(GL_BACK);

	//final render to screen
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//bind shadow map to texture unit 1
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, shadowMapFBODepth);

	glDisable(GL_DEPTH_TEST);
	drawBackground(viewMatrix, projMatrix);
	glEnable(GL_DEPTH_TEST);

	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "lightSpaceMatrix", lightSpaceMatrix);
	labhelper::setUniformSlow(shaderProgram, "shadowMap", 1);

	//draw clouds
	// Animate clouds — drift them in wind direction, wrap around when out of bounds
	float halfWorld = terrainSize;
	float windSpeed = 20.0f; // world units per second
	for (auto& pos : cloudPositions) {
		pos.x += windSpeed * deltaTime;
		// Wrap around so clouds never disappear
		if (pos.x > halfWorld)  pos.x -= halfWorld * 2.0f;
		if (pos.x < -halfWorld) pos.x += halfWorld * 2.0f;
	}
	// Re-upload updated positions to GPU
	glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
		cloudPositions.size() * sizeof(glm::vec3),
		cloudPositions.data());

	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix, vsLightDir);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));

}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while (SDL_PollEvent(&event))
	{
		labhelper::processEvent(&event);

		if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if (labhelper::isGUIvisible())
			{
				labhelper::hideGUI();
			}
			else
			{
				labhelper::showGUI();
			}
		}
		if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
			&& (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if (event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
				normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if (state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if (state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if (state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if (state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if (state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if (state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}
	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
		ImGui::GetIO().Framerate);
	// ----------------------------------------------------------
	ImGui::Separator();

	ImGui::Text("Terrain Settings");
	ImGui::SliderInt("Grass Density", &grassCount, 0, maxGrassCount);
	ImGui::Text("Current Count: %d", grassCount);

	ImGui::Separator();
	ImGui::Text("Day Speed");
	ImGui::SliderFloat("Day Speed", &daySpeed, 0.0f, 10.0f);

	ImGui::Separator();
	ImGui::Text("Water Control");
	ImGui::SliderFloat("Sea Level", &waterHeight, -20.0f, 20.0f);

	ImGui::Separator();
	ImGui::Text("Cloud Density");
	if (ImGui::SliderInt("Cloud count", &numClouds, 0, 60)) {
		cloudPositions.clear();
		cloudModelIndices.clear();  // ADD THIS LINE
		srand(42);
		float halfWorld = terrainSize;
		for (int i = 0; i < numClouds; i++) {
			float x = labhelper::uniform_randf(-halfWorld, halfWorld);
			float z = labhelper::uniform_randf(-halfWorld, halfWorld);
			float y = labhelper::uniform_randf(600.0f, 900.0f);
			cloudPositions.push_back(glm::vec3(x, y, z));
			cloudModelIndices.push_back(rand() % cloudModels.size());  // ADD THIS LINE
		}
		// Re-upload to GPU
		glBindBuffer(GL_ARRAY_BUFFER, cloudPosBuffer);
		glBufferData(GL_ARRAY_BUFFER,
			cloudPositions.size() * sizeof(glm::vec3),
			cloudPositions.data(), GL_DYNAMIC_DRAW);
		// Re-attach instance buffer to VAO
		for (auto* model : cloudModels) {
			glBindVertexArray(model->m_vaob);
			glEnableVertexAttribArray(4);
			glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
			glVertexAttribDivisor(4, 1);
			glBindVertexArray(0);
		}
	}

	//ImGui::End();

	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while (!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// Inform imgui of new frame
		labhelper::newFrame(g_window);

		// render to window
		dayAngle += deltaTime * daySpeed * 0.1f;
		display();

		// Render overlay GUI.
		gui();

		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
