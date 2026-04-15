
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

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap;
const std::string envmap_base_name = "001";

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
float cameraSpeed = 100.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
//labhelper::Model* fighterModel = nullptr;
//labhelper::Model* landingpadModel = nullptr;
//labhelper::Model* sphereModel = nullptr;

mat4 roomModelMatrix;
//mat4 landingPadModelMatrix;
//mat4 fighterModelMatrix;
//mat4 fighterModelMatrix;
//GLuint grassTex, sandTex, rockTex;
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

void initGrassGeometry();
void generateGrass();
void loadShaders(bool is_reload);

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag", is_reload);
	if(shader != 0)
	{
		simpleShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag", is_reload);
	if(shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}
}


///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();
	srand(static_cast<unsigned int>(time(NULL)));
	myTerrain.init(500, 500, 1.5f);
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	//fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	//landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	//sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	//roomModelMatrix = mat4(1.0f);
	//fighterModelMatrix = translate(15.0f * worldUp);
	//landingPadModelMatrix = mat4(1.0f);

	//grassTex = labhelper::loadTexture("../scenes/Grass1.jpg");
	//rockTex = labhelper::loadTexture("../scenes/rock.jpg");
	//sandTex = labhelper::loadTexture("../scenes/sand.jpg");
	grassModel = labhelper::loadModelFromOBJ("../scenes/grass.obj");
	if (grassModel == nullptr) {
		printf("ERROR: grass.obj not found!\n");
	}
	else {
		printf("SUCCESS: Loaded %zu meshes for grass.\n", grassModel->m_meshes.size());
	}
	//initGrassGeometry();
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
	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");


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
	float terrainScale = 1.5f;
	float halfSize = (500.0f / 2.0f) * terrainScale; // 375.0f

	for (int i = 0; i < 10000; i++) { // More grass for a 500x500 map
		float x = labhelper::uniform_randf(-halfSize, halfSize);
		float z = labhelper::uniform_randf(-halfSize, halfSize);

		// Call our new height function
		float h = myTerrain.getHeightAt(x, z, terrainScale);

		// Place grass only on the plains (above sand, below rock)
		if (h > 5.0f && h < 25.0f) {
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

void initGrassGeometry() {
	// Two vertical quads intersecting in an X-shape
	float s = 0.5f; // size
	float h = 1.0f; // height
	float vertices[] = {
		// Quad 1
		-s, 0, 0,  s, h, 0,  -s, h, 0,
		-s, 0, 0,  s, 0, 0,   s, h, 0,
		// Quad 2 (rotated 90 degrees)
		 0, 0, -s,  0, h, s,   0, h, -s,
		 0, 0, -s,  0, 0, s,   0, h, s
	};

	glGenVertexArrays(1, &grassMesh.vao);
	glBindVertexArray(grassMesh.vao);

	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Location 0: Position
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	grassMesh.vertexCount = 12;
	glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


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

	//if (grassMesh.vao != 0 && !grassPositions.empty()) {
	//	glUseProgram(currentShaderProgram);

	//	// 1. Force the uniform BEFORE binding or drawing
	//	labhelper::setUniformSlow(currentShaderProgram, "isGrass", true);
	//	glDisable(GL_CULL_FACE);

	//	// 2. BIND THE VAO FIRST
	//	glBindVertexArray(grassMesh.vao);

	//	// 3. SET UP THE INSTANCE BUFFER while the VAO is bound
	//	glBindBuffer(GL_ARRAY_BUFFER, grassInstanceBuffer);
	//	glEnableVertexAttribArray(4);
	//	glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	//	glVertexAttribDivisor(4, 1);

	//	// 4. DRAW
	//	glDrawArraysInstanced(GL_TRIANGLES, 0, grassMesh.vertexCount, (GLsizei)grassPositions.size());

	//	// 5. CLEANUP
	//	glVertexAttribDivisor(4, 0);
	//	glDisableVertexAttribArray(4);
	//	glBindVertexArray(0);
	//	glEnable(GL_CULL_FACE);
	//	labhelper::setUniformSlow(currentShaderProgram, "isGrass", false);
	//}

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
				(GLsizei)grassPositions.size() // 100,000 instances!
			);
		}

		glBindVertexArray(0);
		glEnable(GL_CULL_FACE);
		labhelper::setUniformSlow(currentShaderProgram, "isGrass", false);
	}

	//map terrain
	//glActiveTexture(GL_TEXTURE10);
	//glBindTexture(GL_TEXTURE_2D, grassTex);

	//glActiveTexture(GL_TEXTURE11);
	//glBindTexture(GL_TEXTURE_2D, sandTex);

	//glActiveTexture(GL_TEXTURE12);
	//glBindTexture(GL_TEXTURE_2D, rockTex);
	//glActiveTexture(GL_TEXTURE0);

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	myTerrain.render();

	// landing pad
	//labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	//                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	//labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	//labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	//                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	//labhelper::render(landingpadModel);

	// Fighter
	//labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	//                          projectionMatrix * viewMatrix * fighterModelMatrix);
	//labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	//labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	//                          inverse(transpose(viewMatrix * fighterModelMatrix)));

	//labhelper::render(fighterModel);
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
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

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE0);


	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		labhelper::perf::Scope s( "Background" );
		drawBackground(viewMatrix, projMatrix);
	}
	{
		labhelper::perf::Scope s( "Scene" );
		drawScene( shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix );
	}
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
	while(SDL_PollEvent(&event))
	{
		labhelper::processEvent( &event );

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if ( labhelper::isGUIvisible() )
			{
				labhelper::hideGUI();
			}
			else
			{
				labhelper::showGUI();
			}
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
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

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
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

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// Inform imgui of new frame
		labhelper::newFrame( g_window );

		// render to window
		display();

		// Render overlay GUI.
		gui();

		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}
	// Free Models
	//labhelper::freeModel(fighterModel);
	//labhelper::freeModel(landingpadModel);
	//labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
