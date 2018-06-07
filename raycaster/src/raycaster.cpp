// Assignment 4, Ray-casting Project
//
// Modify this file according to the lab instructions.
//

#include "utils.h"
#include "utils2.h"
#include "cgVolume.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw_gl3.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <cstdlib>
#include <algorithm>

// The attribute locations we will use in the vertex shader
enum AttributeLocation {
	POSITION = 0,
	NORMAL = 1,
	TEXCOORD = 2,
};

// Struct for representing an indexed triangle mesh
struct Mesh {
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec3> normals;
	std::vector<uint32_t> indices;
};

// Struct for representing a vertex array object (VAO) created from a
// mesh. Used for rendering.
struct MeshVAO {
	GLuint vao;
	GLuint vertexVBO;
	GLuint normalVBO;
	GLuint indexVBO;
	int numVertices;
	int numIndices;
};

// Struct for representing a volume used for ray-casting.
struct RayCastVolume {
	cg::VolumeBase volume;
	GLuint volumeTexture;
	GLuint frontFaceFBO;
	GLuint backFaceFBO;
	GLuint frontFaceTexture;
	GLuint backFaceTexture;

	RayCastVolume() :
		volumeTexture(0),
		frontFaceFBO(0),
		backFaceFBO(0),
		frontFaceTexture(0),
		backFaceTexture(0)
	{}
};

// Struct for resources and state
struct Context {
	int width;
	int height;
	float aspect;
	GLFWwindow *window;
	Trackball trackball;
	Mesh cubeMesh;
	MeshVAO cubeVAO;
	MeshVAO quadVAO;
	GLuint defaultVAO;
	RayCastVolume rayCastVolume;
	GLuint boundingGeometryProgram;
	GLuint rayCasterProgram;
	float angle_of_view;
	float zoom_factor;
	float elapsed_time;
	float ray_step_length;
	glm::vec3 background_color;
	int render_mode;
	float threshold;
	float delta;
	
	glm::vec3 pos_light_pos;
	glm::vec3 pos_light_col;
	glm::vec3 diffuse_color;
	glm::vec3 ambient_color;
	glm::vec3 specular_color;
	float specular_power;
};

// Returns the value of an environment variable
std::string getEnvVar(const std::string &name) {
	char const* value = std::getenv(name.c_str());
	if (value == nullptr) {
		return std::string();
	} else {
		return std::string(value);
	}
}

// Returns the absolute path to the shader directory
std::string shaderDir(void) {
	std::string rootDir = getEnvVar("ASSIGNMENT4_ROOT");
	if (rootDir.empty()) {
		std::cout << "Error: ASSIGNMENT4_ROOT is not set." << std::endl;
		std::exit(EXIT_FAILURE);
	}
	return rootDir + "/raycaster/src/shaders/";
}

// Returns the absolute path to the 3D model directory
std::string modelDir(void) {
	std::string rootDir = getEnvVar("ASSIGNMENT4_ROOT");
	if (rootDir.empty()) {
		std::cout << "Error: ASSIGNMENT4_ROOT is not set." << std::endl;
		std::exit(EXIT_FAILURE);
	}
	return rootDir + "/raycaster/3d_models/";
}

// Returns the absolute path to the cubemap texture directory
std::string cubemapDir(void) {
	std::string rootDir = getEnvVar("ASSIGNMENT4_ROOT");
	if (rootDir.empty()) {
		std::cout << "Error: ASSIGNMENT4_ROOT is not set." << std::endl;
		std::exit(EXIT_FAILURE);
	}
	return rootDir + "/raycaster/cubemaps/";
}

// Returns the absolute path to the volume data directory
std::string volumeDataDir(void) {
	std::string rootDir = getEnvVar("ASSIGNMENT4_ROOT");
	if (rootDir.empty()) {
		std::cout << "Error: ASSIGNMENT4_ROOT is not set." << std::endl;
		std::exit(EXIT_FAILURE);
	}
	return rootDir + "/raycaster/data/";
}

void loadMesh(const std::string &filename, Mesh *mesh) {
	OBJMesh obj_mesh;
	objMeshLoad(obj_mesh, filename);
	mesh->vertices = obj_mesh.vertices;
	mesh->normals = obj_mesh.normals;
	mesh->indices = obj_mesh.indices;
}

void loadRayCastVolume(Context &ctx, const std::string &filename, RayCastVolume *rayCastVolume) {
	cg::VolumeBase volume;
	cg::volumeLoadVTK(&volume, (volumeDataDir() + "/foot.vtk"));
	rayCastVolume->volume = volume;

	glDeleteTextures(1, &rayCastVolume->volumeTexture);
	glGenTextures(1, &rayCastVolume->volumeTexture);
	glBindTexture(GL_TEXTURE_3D, rayCastVolume->volumeTexture);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, volume.dimensions.x,
				 volume.dimensions.y, volume.dimensions.z,
				 0, GL_RED, GL_UNSIGNED_BYTE, &volume.data[0]);
	glBindTexture(GL_TEXTURE_3D, 0);

	glDeleteTextures(1, &rayCastVolume->backFaceTexture);
	glGenTextures(1, &rayCastVolume->backFaceTexture);
	glBindTexture(GL_TEXTURE_2D, rayCastVolume->backFaceTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, ctx.width, ctx.height,
				 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteTextures(1, &rayCastVolume->frontFaceTexture);
	glGenTextures(1, &rayCastVolume->frontFaceTexture);
	glBindTexture(GL_TEXTURE_2D, rayCastVolume->frontFaceTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, ctx.width, ctx.height,
				 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);

	glDeleteFramebuffers(1, &rayCastVolume->frontFaceFBO);
	glGenFramebuffers(1, &rayCastVolume->frontFaceFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, rayCastVolume->frontFaceFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, rayCastVolume->frontFaceTexture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Error: Framebuffer is not complete\n";
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glDeleteFramebuffers(1, &rayCastVolume->backFaceFBO);
	glGenFramebuffers(1, &rayCastVolume->backFaceFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, rayCastVolume->backFaceFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   GL_TEXTURE_2D, rayCastVolume->backFaceTexture, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "Error: Framebuffer is not complete\n";
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void createMeshVAO(Context &ctx, const Mesh &mesh, MeshVAO *meshVAO) {
	// Generates and populates a VBO for the vertices
	glGenBuffers(1, &(meshVAO->vertexVBO));
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
	auto verticesNBytes = mesh.vertices.size() * sizeof(mesh.vertices[0]);
	glBufferData(GL_ARRAY_BUFFER, verticesNBytes, mesh.vertices.data(), GL_STATIC_DRAW);

	// Generates and populates a VBO for the vertex normals
	glGenBuffers(1, &(meshVAO->normalVBO));
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->normalVBO);
	auto normalsNBytes = mesh.normals.size() * sizeof(mesh.normals[0]);
	glBufferData(GL_ARRAY_BUFFER, normalsNBytes, mesh.normals.data(), GL_STATIC_DRAW);

	// Generates and populates a VBO for the element indices
	glGenBuffers(1, &(meshVAO->indexVBO));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshVAO->indexVBO);
	auto indicesNBytes = mesh.indices.size() * sizeof(mesh.indices[0]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indicesNBytes, mesh.indices.data(), GL_STATIC_DRAW);

	// Creates a vertex array object (VAO) for drawing the mesh
	glGenVertexArrays(1, &(meshVAO->vao));
	glBindVertexArray(meshVAO->vao);
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
	glEnableVertexAttribArray(POSITION);
	glVertexAttribPointer(POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->normalVBO);
	glEnableVertexAttribArray(NORMAL);
	glVertexAttribPointer(NORMAL, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshVAO->indexVBO);
	glBindVertexArray(ctx.defaultVAO);

	// Additional information required by draw calls
	meshVAO->numVertices = mesh.vertices.size();
	meshVAO->numIndices = mesh.indices.size();
}

void createQuadVAO(Context &ctx, MeshVAO *meshVAO) {
	const glm::vec3 vertices[] = {
		glm::vec3(-1.0f, -1.0f,  0.0f),
		glm::vec3(1.0f, -1.0f,  0.0f),
		glm::vec3(1.0f,  1.0f,  0.0f),
		glm::vec3(-1.0f, -1.0f,  0.0f),
		glm::vec3(1.0f,  1.0f,  0.0f),
		glm::vec3(-1.0f,  1.0f,  0.0f),
	};

	// Generates and populates a VBO for the vertices
	glGenBuffers(1, &(meshVAO->vertexVBO));
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
	auto verticesNBytes = 6 * sizeof(vertices[0]);
	glBufferData(GL_ARRAY_BUFFER, verticesNBytes, &vertices[0], GL_STATIC_DRAW);

	// Creates a vertex array object (VAO) for drawing the mesh
	glGenVertexArrays(1, &(meshVAO->vao));
	glBindVertexArray(meshVAO->vao);
	glBindBuffer(GL_ARRAY_BUFFER, meshVAO->vertexVBO);
	glEnableVertexAttribArray(POSITION);
	glVertexAttribPointer(POSITION, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
	glBindVertexArray(ctx.defaultVAO);

	// Additional information required by draw calls
	meshVAO->numVertices = 6;
	meshVAO->numIndices = 0;
}

void initializeTrackball(Context &ctx) {
	double radius = double(std::min(ctx.width, ctx.height)) / 2.0;
	ctx.trackball.radius = radius;
	glm::vec2 center = glm::vec2(ctx.width, ctx.height) / 2.0f;
	ctx.trackball.center = center;
}

void init(Context &ctx) {
	// Load shaders
	ctx.boundingGeometryProgram = loadShaderProgram(shaderDir() + "boundingGeometry.vert",
													shaderDir() + "boundingGeometry.frag");
	ctx.rayCasterProgram = loadShaderProgram(shaderDir() + "rayCaster.vert",
											 shaderDir() + "rayCaster.frag");
	ctx.render_mode = 0;
	ctx.angle_of_view = glm::radians(45.0f);
	ctx.zoom_factor = 1.0f;
	ctx.aspect = float(ctx.width) / float(ctx.height);
	ctx.background_color = glm::vec3(0.0f);
	ctx.ray_step_length = 0.001;
	ctx.threshold = 0.35;
	ctx.delta = 0.01;

	ctx.pos_light_pos = glm::vec3(5.f, 2.0f, .0f);
	ctx.pos_light_col = glm::vec3(1.f); // Positional light color, white
	ctx.diffuse_color = glm::vec3(0.5f);
	ctx.ambient_color = glm::vec3(0.0f);
	ctx.specular_color = glm::vec3(0.1f, 0.12f, 0.1f);
	ctx.specular_power = 2.5f;

	// Load bounding geometry (2-unit cube)
	loadMesh((modelDir() + "cube.obj"), &ctx.cubeMesh);
	createMeshVAO(ctx, ctx.cubeMesh, &ctx.cubeVAO);

	// Create fullscreen quad for ray-casting
	createQuadVAO(ctx, &ctx.quadVAO);

	// Load volume data
	loadRayCastVolume(ctx, (volumeDataDir() + "/foot.vtk"), &ctx.rayCastVolume);
	ctx.rayCastVolume.volume.spacing *= 0.008f;  // FIXME

	initializeTrackball(ctx);
}

// MODIFY THIS FUNCTION
void drawBoundingGeometry(Context &ctx, GLuint program, const MeshVAO &cubeVAO,
						  const RayCastVolume &rayCastVolume) {
	glm::mat4 model = cg::volumeComputeModelMatrix(rayCastVolume.volume);
	model = trackballGetRotationMatrix(ctx.trackball) * model;
	glm::mat4 view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.0f));
	glm::mat4 projection = glm::perspective(ctx.angle_of_view * ctx.zoom_factor, ctx.aspect, 0.1f, 100.0f);
	glm::mat4 mvp = projection * view * model;
	glm::mat4 mv = view * model;

	glUseProgram(program);

	glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, &mvp[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(program, "u_mv"), 1, GL_FALSE, &mv[0][0]);

	glBindVertexArray(cubeVAO.vao);
	glDrawElements(GL_TRIANGLES, cubeVAO.numIndices, GL_UNSIGNED_INT, 0);
	glBindVertexArray(ctx.defaultVAO);

	glUseProgram(0);
}

// MODIFY THIS FUNCTION
void drawRayCasting(Context &ctx, GLuint program, const MeshVAO &quadVAO, 
					const RayCastVolume &rayCastVolume) {

	glUseProgram(program);

	// Set uniforms and bind textures here...
	// Uniforms

	glUniform1i(glGetUniformLocation(program, "u_render_mode"), ctx.render_mode);
	glUniform1f(glGetUniformLocation(program, "u_ray_step"), ctx.ray_step_length);
	glUniform1f(glGetUniformLocation(program, "u_threshold"), ctx.threshold);
	glUniform1f(glGetUniformLocation(program, "u_delta"), ctx.delta);
    
   	glUniform3fv(glGetUniformLocation(program, "u_background_color"), 1, &ctx.background_color[0]);

	glUniform3fv(glGetUniformLocation(program, "u_pos_light_pos"), 1, &ctx.pos_light_pos[0]);
	glUniform3fv(glGetUniformLocation(program, "u_pos_light_col"), 1, &ctx.pos_light_col[0]);
	glUniform3fv(glGetUniformLocation(program, "u_ambient_color"), 1, &ctx.ambient_color[0]);
	glUniform3fv(glGetUniformLocation(program, "u_diffuse_color"), 1, &ctx.diffuse_color[0]);
	glUniform3fv(glGetUniformLocation(program, "u_specular_color"), 1, &ctx.specular_color[0]);
	glUniform1f(glGetUniformLocation(program, "u_specular_power"), ctx.specular_power);

	// Textures
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ctx.rayCastVolume.frontFaceTexture);
	glUniform1i(glGetUniformLocation(program, "u_frontFaceTexture"), 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, ctx.rayCastVolume.backFaceTexture);
	glUniform1i(glGetUniformLocation(program, "u_backFaceTexture"), 1);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_3D, ctx.rayCastVolume.volumeTexture);
	glUniform1i(glGetUniformLocation(program, "u_volumeTexture"), 2);

	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindVertexArray(quadVAO.vao);
	glDrawArrays(GL_TRIANGLES, 0, quadVAO.numVertices);
	glBindVertexArray(ctx.defaultVAO);

	glUseProgram(0);
}

void display(Context &ctx) {
	glClearColor(ctx.background_color[0], ctx.background_color[1], ctx.background_color[2], 0.0f); //specify clear values for the color buffers RGBA
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear buffers to preset values

	// Render the front faces of the volume bounding box to a texture via the frontFaceFBO
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glBindFramebuffer(GL_FRAMEBUFFER, ctx.rayCastVolume.frontFaceFBO);
	glClear(GL_COLOR_BUFFER_BIT);
	drawBoundingGeometry(ctx, ctx.boundingGeometryProgram, ctx.cubeVAO, ctx.rayCastVolume);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Render the back faces of the volume bounding box to a texture via the backFaceFBO
	glCullFace(GL_FRONT);
	glBindFramebuffer(GL_FRAMEBUFFER, ctx.rayCastVolume.backFaceFBO);
	glClear(GL_COLOR_BUFFER_BIT);
	
	drawBoundingGeometry(ctx, ctx.boundingGeometryProgram, ctx.cubeVAO, ctx.rayCastVolume);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear buffers to preset values
	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);

	glEnable(GL_DEPTH_TEST);
	drawRayCasting(ctx, ctx.rayCasterProgram, ctx.quadVAO, ctx.rayCastVolume);
}

void reloadShaders(Context *ctx) {
	glDeleteProgram(ctx->boundingGeometryProgram);
	ctx->boundingGeometryProgram = loadShaderProgram(shaderDir() + "boundingGeometry.vert",
													 shaderDir() + "boundingGeometry.frag");
	glDeleteProgram(ctx->rayCasterProgram);
	ctx->rayCasterProgram = loadShaderProgram(shaderDir() + "rayCaster.vert",
											  shaderDir() + "rayCaster.frag");
}

void mouseButtonPressed(Context *ctx, int button, int x, int y) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		ctx->trackball.center = glm::vec2(x, y);
		trackballStartTracking(ctx->trackball, glm::vec2(x, y));
	}
}

void mouseButtonReleased(Context *ctx, int button, int x, int y) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		trackballStopTracking(ctx->trackball);
	}
}

void moveTrackball(Context *ctx, int x, int y) {
	if (ctx->trackball.tracking) {
		trackballMove(ctx->trackball, glm::vec2(x, y));
	}
}

void errorCallback(int /*error*/, const char* description) {
	std::cerr << description << std::endl;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	// Forward event to GUI
	ImGui_ImplGlfwGL3_KeyCallback(window, key, scancode, action, mods);
	if (ImGui::GetIO().WantCaptureKeyboard) { 
		return;  // Skip other handling
	} else {
		Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
		if (key == GLFW_KEY_R && action == GLFW_PRESS) {
			reloadShaders(ctx);
		} else if (key == GLFW_KEY_Q && action == GLFW_PRESS) {
			glfwDestroyWindow(ctx->window);
			glfwTerminate();
			std::exit(EXIT_SUCCESS);
		}
	}
}

void charCallback(GLFWwindow* window, unsigned int codepoint) {
	// Forward event to GUI
	ImGui_ImplGlfwGL3_CharCallback(window, codepoint);
	if (ImGui::GetIO().WantTextInput) {
		return;
	} else {

	}
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	// Forward event to GUI
	ImGui_ImplGlfwGL3_MouseButtonCallback(window, button, action, mods);
	if (ImGui::GetIO().WantCaptureMouse) { 
		return;  // Skip other handling
	} else {
		double x, y;
		glfwGetCursorPos(window, &x, &y);

		Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
		if (action == GLFW_PRESS) {
			mouseButtonPressed(ctx, button, x, y);
		} else {
			mouseButtonReleased(ctx, button, x, y);
		}
	}
}

void cursorPosCallback(GLFWwindow* window, double x, double y) {
	if (ImGui::GetIO().WantCaptureMouse) { return; }  // Skip other handling   

	Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
	moveTrackball(ctx, x, y);
}

void resizeCallback(GLFWwindow* window, int width, int height) {
	Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
	ctx->width = width;
	ctx->height = height;
	ctx->aspect = float(width) / float(height);
	ctx->trackball.radius = double(std::min(width, height)) / 2.0;
	ctx->trackball.center = glm::vec2(width, height) / 2.0f;
	glViewport(0, 0, width, height);

	// Resize FBO textures to match window size
	glBindTexture(GL_TEXTURE_2D, ctx->rayCastVolume.frontFaceTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, ctx->width, ctx->height,
				 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
	glBindTexture(GL_TEXTURE_2D, ctx->rayCastVolume.backFaceTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, ctx->width, ctx->height,
				 0, GL_RGBA, GL_UNSIGNED_SHORT, nullptr);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void scrollCallback(GLFWwindow* window, double x, double y) {
	if (ImGui::GetIO().WantCaptureMouse) { 
		return; // Skip other handling
	} else {
		Context *ctx = static_cast<Context *>(glfwGetWindowUserPointer(window));
		ctx->zoom_factor = ctx->zoom_factor + 0.03f*(float)y;
	}
}

int main(void) {
	Context ctx;

	// Create a GLFW window
	glfwSetErrorCallback(errorCallback);
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	// Initialize context variables
	ctx.width = 1500;
	ctx.height = 1500;
	ctx.window = glfwCreateWindow(ctx.width, ctx.height, "Volume rendering", nullptr, nullptr);
	// Initialize window object
	glfwMakeContextCurrent(ctx.window);
	glfwSetWindowUserPointer(ctx.window, &ctx);
	glfwSetKeyCallback(ctx.window, keyCallback);
	glfwSetCharCallback(ctx.window, charCallback);
	glfwSetMouseButtonCallback(ctx.window, mouseButtonCallback);
	glfwSetCursorPosCallback(ctx.window, cursorPosCallback);
	glfwSetFramebufferSizeCallback(ctx.window, resizeCallback);
	glfwSetScrollCallback(ctx.window, scrollCallback);

	// Load OpenGL functions
	glewExperimental = true;
	GLenum status = glewInit();
	if (status != GLEW_OK) {
		std::cerr << "Error: " << glewGetErrorString(status) << std::endl;
		std::exit(EXIT_FAILURE);
	}
	std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;

	// Initialize GUI
	ImGui_ImplGlfwGL3_Init(ctx.window, false /*do not install callbacks*/);

	// Initialize rendering
	glGenVertexArrays(1, &ctx.defaultVAO);
	glBindVertexArray(ctx.defaultVAO);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	init(ctx);

	// Start rendering loop
	while (!glfwWindowShouldClose(ctx.window)) {
		glfwPollEvents();
		ctx.elapsed_time = glfwGetTime();
		ImGui_ImplGlfwGL3_NewFrame();
		display(ctx);
		ImGui::ColorEdit3("Background color", &ctx.background_color[0]); // change background color
		ImGui::SliderFloat("Step size", &ctx.ray_step_length, 0.001f, 0.1f); // ray-step length
		if (ImGui::Button("Switch mode")) { // switch ray-casting mode
			ctx.render_mode = (ctx.render_mode == 0) ? 1 : 0;
		}
		// manipulate transfer function (not for iso surface rendering)
		if (ctx.render_mode == 1) {
			ImGui::SliderFloat("Threshold", &ctx.threshold, 0.0f, 1.0f);
			ImGui::SliderFloat("Delta", &ctx.delta, 0.001f, 0.1f);
			ImGui::ColorEdit3("Positional light", &ctx.pos_light_col[0]);
			ImGui::ColorEdit3("Diffuse light", &ctx.diffuse_color[0]);
			ImGui::ColorEdit3("Ambient light", &ctx.ambient_color[0]);
			ImGui::ColorEdit3("Specular light", &ctx.specular_color[0]);
			ImGui::SliderFloat("Specular power", &ctx.specular_power, 0.0f, 100.0f);
		}
		ImGui::Render();
		glfwSwapBuffers(ctx.window);
	}

	// Shutdown
	glfwDestroyWindow(ctx.window);
	glfwTerminate();
	std::exit(EXIT_SUCCESS);
}
