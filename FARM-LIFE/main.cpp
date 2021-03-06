/* Add dependencies
	* SOIL - Library for loading textures (Simple OpenGL Image Library)
	* GLEW - OpenGL Extension Wrangler
	* GLFW - Graphics Library Framework
	* GLM - OpenGL Mathematics
	* OpenAL - Open Audio Library
	*/
#include <windows.h>
#include <sdl.h>
#include <SOIL.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "al.h"
#include "alc.h"

	// Include the standard C++ headers
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <random>

// Include project files
#include "util/mainUtil.hpp"
#include "util/camera.hpp"
#include "lights/lights.hpp"
#include "audio/audio.hpp"
#include "terrain/terrain.hpp"
#include "models/model.hpp"
#include "models/paddock/paddock.hpp"
#include "skybox/skybox.hpp"
#include "water/water.hpp"
#include "water/WaterFrameBuffers.hpp"
#include "trees/tree.hpp"

// Initial width and height of the window
GLuint SCREEN_WIDTH = 1200;
GLuint SCREEN_HEIGHT = 800;

// Distances to the near and the far plane. Used for the camera to clip space transform.
static constexpr float NEAR_PLANE = 0.1f;
static constexpr float FAR_PLANE = 1000.0f;

std::vector<model::Model*> models;	// vector of all models to render
std::vector<model::Model*> SLmodels;
std::vector<model::HitBox> hitBoxes; // vector of all hitboxes in the scene for collision detections
std::vector<model::Paddock*> paddocks;  // vector of all paddocks for use with moveable gates
std::vector<model::Model*> lostCat;
static int debounceCounter = 0;		 // simple counter to debounce keyboard inputs

//Amount cat has been caught
int catCaught = 0;

void foundTheCat(utility::camera::Camera& camera, float terrainHeight, terrain::Terrain terra) {
	model::Model* cat = lostCat[0];
	float lxCoord, lyCoord, lzCoord;
	float sxCoord, syCoord, szCoord;
	//xCoord = cat->getPosition().x - camera.get_position().x;
	//yCoord = cat->getPosition().y - camera.get_position().y;
	//zCoord = cat->getPosition().z - camera.get_position().z;

	if (cat->getPosition().x > camera.get_position().x) {
		lxCoord = cat->getPosition().x;
		sxCoord = camera.get_position().x;
	}
	else {
		lxCoord = camera.get_position().x;
		sxCoord = cat->getPosition().x;
	}
	/*
	if (cat->getPosition().y > camera.get_position().y) {
		lyCoord = cat->getPosition().y;
		syCoord = camera.get_position().y;
	}
	else {
		lyCoord = camera.get_position().y;
		syCoord = cat->getPosition().y;
	}
	*/
	if (cat->getPosition().z > camera.get_position().z) {
		lzCoord = cat->getPosition().z;
		szCoord = camera.get_position().z;
	}
	else {
		lzCoord = camera.get_position().z;
		szCoord = cat->getPosition().z;
	}

	if ((lxCoord - sxCoord) < 6.0f && (lzCoord - szCoord) < 6.0f) { // (lzCoord - szCoord) < 3.0f
		catCaught++;
		float xCoordNew, yCoordNew, zCoordNew;
		float modelHeightInWorld;
		if (catCaught < 6) {

			std::random_device randCoord;
			std::mt19937 rng(randCoord());
			std::uniform_int_distribution<std::mt19937::result_type> dist500(1, 500); // distribution in range [1, 500]
			//For some reason putting it as yCoordNew = dist500(rng)-250; would occasionally give a numbner  like 4.0928+e09 for the Y(actaully z)
			//End randomised values in between -250 to 250
			float randX = dist500(rng);
			xCoordNew = randX - 250;
			float randY = dist500(rng);
			yCoordNew = randY - 250;

			std::cout << " " << randY << std::endl;
			modelHeightInWorld = cat->GetModelTerrainHeight(terra, xCoordNew, yCoordNew, 500.0f, 500.0f, -20.0f);

			cat->ShiftTo(glm::vec3(xCoordNew, modelHeightInWorld, yCoordNew));
		}
		else {
			xCoordNew = 78;
			yCoordNew = 158;
			modelHeightInWorld = cat->GetModelTerrainHeight(terra, xCoordNew, yCoordNew, 500.0f, 500.0f, -20.0f);
			cat->ShiftTo(glm::vec3(xCoordNew, modelHeightInWorld, yCoordNew));
		}
		//This let's us know where the cat is for easier finding
		std::cout << " New Position of Cat: " << xCoordNew << " " << modelHeightInWorld << " " << yCoordNew << " " << std::endl;


	}

}


void checkPaddockGates(utility::camera::Camera& camera)
{
	for (model::Paddock* paddock : paddocks)
	{
		model::Model* gate = paddock->GetGate();

		bool xCheck, yCheck, zCheck;
		float xBound, yBound, zBound;
		if (paddock->GateOpenStatus())
		{
			// Gate is currently open
			xBound = 2 * gate->hitBox.size.z;
			yBound = 2 * gate->hitBox.size.y;
			zBound = 3 * gate->hitBox.size.z;
		}
		else
		{
			// Gate is currently closed
			xBound = gate->hitBox.size.x;
			yBound = 2 * gate->hitBox.size.y;
			zBound = 2 * gate->hitBox.size.x;
		}

		// Check each axis for sufficient distance between the model hitbox and the camera hitbox
		xCheck = abs(camera.get_position().x - gate->position.x) < xBound;
		if (!xCheck) continue;
		yCheck = abs(camera.get_position().y - gate->position.y) < yBound;
		if (!yCheck) continue;
		zCheck = abs(camera.get_position().z - gate->position.z) < zBound;
		if (!zCheck) continue;

		// Open/Close Gate
		paddock->ToggleGate(models);
		// Correct gate has been toggled
		break;
	}
}

void process_input(GLFWwindow* window, const float& delta_time, utility::camera::Camera& camera, float terrainHeight, terrain::Terrain terra)
{
	// Movement sensitivity is updated base on the delta_time and not framerate, gravity accelleration is also based on delta_time
	camera.set_movement_sensitivity(30.0f * delta_time);
	camera.gravity(delta_time, terrainHeight); // apply gravity, giving the floor of the current (x,y) position

	// Process movement inputs
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, true);
	}
	else if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		camera.move_forward(hitBoxes);
	}
	else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		camera.move_backward(hitBoxes);
	}
	else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		camera.move_left(hitBoxes);
	}
	else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		camera.move_right(hitBoxes);
	}
	else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
	{
		// Check if gate should be opened
		checkPaddockGates(camera);
	}
	else if (glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS)
	{
		// Check if the cat was found, the cout is to make it easier to locate the cat relative to you.
		std::cout << " Your current postion is here: " << camera.get_position().x << " " << camera.get_position().y << " " << camera.get_position().z << " ";
		foundTheCat(camera, terrainHeight, terra);
	}
	else if (glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS)
	{
		// Check if the cat was found, the cout is to make it easier to locate the cat relative to you.
		double timer = 0;
		glfwSetTime(timer);
	}

	//void glfwSetTime	(	double 	time	)	


	// Process debounced inputs - this ensures we won't have 5 jump events triggering before we leave the ground etc.
	if (debounceCounter == 0)
	{
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		{
			camera.jump(delta_time, terrainHeight);
		}
		else if (glfwGetKey(window, GLFW_KEY_GRAVE_ACCENT) == GLFW_PRESS)
		{
			camera.toggleNoClip();
		}
		else if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS)
		{
			std::cout << "Model Hitbox: " << std::endl;
			std::cout << hitBoxes[0].origin.x << " " << hitBoxes[0].origin.y << " " << hitBoxes[0].origin.z << std::endl;
			std::cout << hitBoxes[0].size.x << " " << hitBoxes[0].size.y << " " << hitBoxes[0].size.z << std::endl;

			model::HitBox cameraHitBox = camera.getHitBox();

			std::cout << "Model Hitbox: " << std::endl;
			std::cout << cameraHitBox.origin.x << " " << cameraHitBox.origin.y << " " << cameraHitBox.origin.z << std::endl;
			std::cout << cameraHitBox.size.x << " " << cameraHitBox.size.y << " " << cameraHitBox.size.z << std::endl;
		}
	}
	// debounce inputs
	if (debounceCounter == 5)
	{
		debounceCounter = 0;
	}
	else
	{
		debounceCounter++;
	}
}

void render(terrain::Terrain terra, utility::camera::Camera camera, std::vector<model::Model*> models, skybox::Skybox skybox, GLuint modelShader, glm::vec4 clippingPlane, std::vector<model::Model*> SLmodels, GLuint streetLightShader)
{
	// get the camera transforms and position
	glm::mat4 Hvw = camera.get_view_transform();
	glm::mat4 Hcv = camera.get_clip_transform();
	glm::mat4 Hwm = glm::mat4(1.0f);
	glm::vec3 CamPos = camera.get_position();
	// Clear color buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// NOTE: Draw all other objects before the skybox

	// Draw the models
	glDepthFunc(GL_LESS);
	glm::vec3 Forward = camera.get_view_direction();
	for (int i = 0; i < models.size(); i++)
	{
		models[i]->Draw(modelShader, Hvw, Hcv, Hwm, clippingPlane, CamPos, Forward);
	}

	// Draw the Street Orbs
	for (int i = 0; i < SLmodels.size(); i++)
	{
		SLmodels[i]->Draw(streetLightShader, Hvw, Hcv, Hwm, clippingPlane, CamPos, Forward);
	}

	// Render skybox last, disable clipping for skybox
	glDisable(GL_CLIP_DISTANCE0);
	glm::mat4 skybox_Hvw = glm::mat4(glm::mat3(camera.get_view_transform())); // remove translation from the view matrix. Keeps the skybox centered on camera.
	skybox.render(skybox_Hvw, Hcv);
	glEnable(GL_CLIP_DISTANCE0);

	//-------------
	// DRAW TERRAIN
	//-------------
	terra.draw(Hvw, Hcv, clippingPlane, camera.get_position(), glm::vec3(0.0, 50, 0.0), glm::vec3(1.0, 1.0, 1.0),
		glfwGetTime(), Forward);
}

// Loads a loading screen for FARM-LIFE: GAME OF THE YEAR EDITION
void addLoadingScreen()
{
	// Create and bind vertex array object
	GLuint vao1;
	glGenVertexArrays(1, &vao1);
	glBindVertexArray(vao1);

	// Create vertex buffer object
	float vertices[] = {
		-1.0f, 1.0f, 0.0f, 0.0f, // upper left
		1.0f, 1.0f, 1.0f, 0.0f,  // upper right
		1.0f, -1.0f, 1.0f, 1.0f, // lower right
		-1.0f, -1.0f, 0.0f, 1.0f // lower left
	};

	GLuint vbo1;
	glGenBuffers(1, &vbo1);
	glBindBuffer(GL_ARRAY_BUFFER, vbo1);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	// Create and bind element buffer object
	int elements[] = {
		0, 1, 2,
		2, 3, 0 };

	GLuint ebo1;
	glGenBuffers(1, &ebo1);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo1);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	// Create shader program
	GLuint shader1 = LoadShaders("shaders/loading.vert", "shaders/loading.frag");

	glUseProgram(shader1);

	// Create and bind texture
	int width, height; // Variables for the width and height of image being loaded
	GLuint tex1;
	glGenTextures(1, &tex1);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex1);
	unsigned char* image = SOIL_load_image("loading_screen.png", &width, &height, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
		GL_UNSIGNED_BYTE, image);
	glUniform1i(glGetUniformLocation(shader1, "screen"), 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	// Set vertex attributes
	GLuint posAttrib = glGetAttribLocation(shader1, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), 0);

	GLuint texAttrib = glGetAttribLocation(shader1, "texcoords");
	glEnableVertexAttribArray(texAttrib);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float), (void*)(2 * sizeof(float)));
}

int main(void)
{
	// Set the screen size by the current desktop height, width (for fullscreen)
	//setScreenSize(SCREEN_WIDTH, SCREEN_HEIGHT);

	std::srand(1);
	utility::camera::Camera camera(SCREEN_WIDTH, SCREEN_HEIGHT, NEAR_PLANE, FAR_PLANE);

	//Set the error callback
	glfwSetErrorCallback(error_callback);

	//Initialize GLFW
	if (!glfwInit())
	{
		exit(EXIT_FAILURE);
	}

	//Set the GLFW window creation hints - these are optional
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);				   //Request a specific OpenGL version
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);				   //Request a specific OpenGL version
	glfwWindowHint(GLFW_SAMPLES, 4);							   //Request 4x antialiasing
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //modern opengl

	//Declare a window object
	GLFWwindow* window;

	// Create a window and create its OpenGL context, creates a fullscreen window using glfwGetPrimaryMonitor(), requires a monitor for fullscreen
	//window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Farm-Life: GOTY Edition", glfwGetPrimaryMonitor(), NULL);

	//USE THIS LINE INSTEAD OF LINE ABOVE IF GETTING RUNTIME ERRORS
	window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Farm-Life: GOTY Edition", NULL, NULL);

	if (window == NULL)
	{
		std::cerr << "Failed to create GLFW window with dimension " << SCREEN_WIDTH << SCREEN_HEIGHT
			<< std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window,
		CCallbackWrapper(GLFWframebuffersizefun, utility::camera::Camera)(
			std::bind(&utility::camera::Camera::framebuffer_size_callback,
				&camera,
				std::placeholders::_1,
				std::placeholders::_2,
				std::placeholders::_3)));

	// get glfw to capture and hide the mouse pointer
	// ----------------------------------------------
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	glfwSetCursorPosCallback(
		window,
		CCallbackWrapper(GLFWcursorposfun, utility::camera::Camera)(std::bind(&utility::camera::Camera::mouse_callback,
			&camera,
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3)));

	// get glfw to capture mouse scrolling
	// -----------------------------------
	glfwSetScrollCallback(
		window,
		CCallbackWrapper(GLFWscrollfun, utility::camera::Camera)(std::bind(&utility::camera::Camera::scroll_callback,
			&camera,
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3)));

	//Sets the key callback
	glfwSetKeyCallback(window, key_callback);

	//Initialize GLEW
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();

	//If GLEW hasn't initialized
	if (err != GLEW_OK)
	{
		fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		return -1;
	}

	// Initialise AL
	ALCdevice* device = alcOpenDevice(NULL);
	if (device == NULL)
	{
		std::cout << "cannot open sound card" << std::endl;
	}
	if (!device)
	{
		std::cout << "not device" << std::endl;
	}
	ALCcontext* context = alcCreateContext(device, NULL);
	if (context == NULL)
	{
		std::cout << "cannot open context" << std::endl;
	}
	if (!context)
	{
		std::cout << "not context" << std::endl;
	}

	alcMakeContextCurrent(context);
	alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);

	//--------------------------------------------------------
	//-----------COMPLETED DEPENDENCY INITITIALISATION--------
	//------------------INITIALISE SCENE----------------------
	//--------------------------------------------------------

	// Draw screen while waiting for the main program to load
	addLoadingScreen();
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glfwSwapBuffers(window);

	//---------------------
	// SET BACKGROUND MUSIC
	//---------------------
	audio::setListener(camera.get_position());
	audio::Source camSource = audio::Source();
	camSource.setLooping(true);
	camSource.setPosition(camera.get_position());
	camSource.setVolume(0.07);
	GLuint mainMusic = audio::loadAudio("audio/bensound-acousticbreeze.wav");
	camSource.play(mainMusic);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);

	//---------------
	// CREATE TERRAIN
	//---------------
	// Set up variables and initialize a terrain instance
	int tresX = 1000; // x and y resolutions for the terrain, keep in a variable for use in other calculations
	int tresY = 1000;
	float terraScale = 1.0;
	int terraMaxHeight = 30;
	float terraYOffset = -20.0f; // the terrain is offset in the y by terraYOffset
	// Create main terrain
	terrain::Terrain terra = terrain::Terrain(tresX, tresY, terraScale, terraMaxHeight, terraYOffset, terraMaxHeight / 2.5);
	// Create water frame buffers for reflection and refraction
	water::WaterFrameBuffers fbos = water::WaterFrameBuffers();
	// Create water
	water::Water water = water::Water(tresX, tresY, terraScale, terraMaxHeight / 2.5, fbos);
	water.playSound("audio/river.wav");

	// Set up the camera offset, terrain is from (-500,-500) to (500,500) in the world, camera range is (0,0) to (1000,1000)
	int cameraOffsetX = tresX / 2;
	int cameraOffsetY = tresY / 2;
	//Enable lighting

	glEnable(GL_LIGHTING);
	//glDisable(GL_LIGHTING); To disable the lighting for the skybox later

	//--------------
	// CREATE MODELS
	//--------------

	// Load the shaders to be used in the scene
	//GLuint shaderProgram = LoadShaders("shaders/shader.vert", "shaders/shader.frag");
	GLuint lightShader = LoadShaders("shaders/light.vert", "shaders/light.frag");
	GLuint modelShader = LoadShaders("shaders/model.vert", "shaders/model.frag");
	GLuint streetLightShader = LoadShaders("shaders/SLmodel.vert", "shaders/SLmodel.frag");

	int modelXCoord, modelYCoord;
	float modelHeightInWorld;
	int paddockXCoord, paddockYCoord;

	//**********************************************Street light Orbs********************************************************************************
	//remember to change the position of both the post and the orb parts if moving the lights, as well as adjusting the light position in the lights.hpp
	//Light number 1
	model::Model* streetLightOrb1 = new model::Model("models/StreetLight/StreetLightMetallicOrb.obj");
	modelXCoord = 1;
	modelYCoord = 2;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightOrb1->hitBox.size.y) - 1;
	streetLightOrb1->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	SLmodels.push_back(streetLightOrb1);
	hitBoxes.push_back(streetLightOrb1->hitBox);
	//Light number 2
	model::Model* streetLightOrb2 = new model::Model("models/StreetLight/StreetLightMetallicOrb.obj");
	modelXCoord = 50;
	modelYCoord = 80;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightOrb2->hitBox.size.y) - 1;
	streetLightOrb2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	SLmodels.push_back(streetLightOrb2);
	hitBoxes.push_back(streetLightOrb2->hitBox);
	//Light number 3
	model::Model* streetLightOrb3 = new model::Model("models/StreetLight/StreetLightMetallicOrb.obj");
	modelXCoord = 70;
	modelYCoord = 145;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightOrb3->hitBox.size.y) - 1;
	//std::cout << "Light height: " << modelHeightInWorld - 3<<" ";
	streetLightOrb3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	SLmodels.push_back(streetLightOrb3);
	hitBoxes.push_back(streetLightOrb3->hitBox);
	//Light number 4
	model::Model* streetLightOrb4 = new model::Model("models/StreetLight/StreetLightMetallicOrb.obj");
	modelXCoord = 10;
	modelYCoord = 50;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightOrb4->hitBox.size.y) - 1;
	streetLightOrb4->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	SLmodels.push_back(streetLightOrb4);
	hitBoxes.push_back(streetLightOrb4->hitBox);
	//**********************************************Street lights Orbs********************************************************************************

	//**********************************************Street light Posts********************************************************************************
	//Light number 1
	model::Model* streetLightPost1 = new model::Model("models/StreetLight/StreetLightPost.obj");
	modelXCoord = 1;
	modelYCoord = 2;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightPost1->hitBox.size.y) - 3;
	//std::cout << " " << modelHeightInWorld << " "; //1.3
	streetLightPost1->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(streetLightPost1);
	hitBoxes.push_back(streetLightPost1->hitBox);
	//Light number 2
	model::Model* streetLightPost2 = new model::Model("models/StreetLight/StreetLightPost.obj");
	modelXCoord = 50;
	modelYCoord = 80;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightPost2->hitBox.size.y) - 3;
	//std::cout << " " << modelHeightInWorld << " "; //1.3
	streetLightPost2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(streetLightPost2);
	hitBoxes.push_back(streetLightPost2->hitBox);
	//Light number 3
	model::Model* streetLightPost3 = new model::Model("models/StreetLight/StreetLightPost.obj");
	modelXCoord = 70;
	modelYCoord = 145;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightPost3->hitBox.size.y) - 3;
	//std::cout << " " << modelHeightInWorld << " "; //1.3
	streetLightPost3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(streetLightPost3);
	hitBoxes.push_back(streetLightPost3->hitBox);
	//Light number 4
	model::Model* streetLightPost4 = new model::Model("models/StreetLight/StreetLightPost.obj");
	modelXCoord = 10;
	modelYCoord = 50;
	modelHeightInWorld = terra.getHeightAt(modelXCoord + cameraOffsetX, modelYCoord + cameraOffsetY) + terraYOffset + (streetLightPost4->hitBox.size.y) - 3;
	//std::cout << " " << modelHeightInWorld << " "; //1.3
	streetLightPost4->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(streetLightPost4);
	hitBoxes.push_back(streetLightPost4->hitBox);
	//**********************************************Street lights Posts********************************************************************************




	model::Model* barn = new model::Model("models/barn/barn.obj");
	modelXCoord = 82, modelYCoord = 110;
	modelHeightInWorld = barn->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 4.0f;
	barn->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(barn);
	hitBoxes.push_back(barn->hitBox);

	tree::Tree tree = tree::Tree("trees/placemap.bmp", terra);
	for (int i = 0; i < 30; i++)
	{
		models.push_back(tree.placeTree(i));
		hitBoxes.push_back(tree.placeTree(i)->hitBox);
	};

	model::Model* bucket = new model::Model("models/bucket/bucket.obj");
	modelXCoord = 89, modelYCoord = 118;
	modelHeightInWorld = bucket->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset);
	bucket->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(bucket);
	hitBoxes.push_back(bucket->hitBox);

	/* Cat Paddock */
	model::Paddock* paddock2 = new model::Paddock(4, 3);
	paddockXCoord = 70, paddockYCoord = 140;
	paddock2->MovePaddock(glm::vec2(paddockXCoord, paddockYCoord), terra, cameraOffsetX, cameraOffsetY, terraYOffset);
	paddocks.push_back(paddock2);
	paddock2->PushModels(models);
	paddock2->PushHitBoxes(hitBoxes);

	model::Model* trough = new model::Model("models/trough/watertrough.obj");
	modelXCoord = 101, modelYCoord = 148;
	modelHeightInWorld = trough->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) + 0.5f;
	trough->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(trough);
	hitBoxes.push_back(trough->hitBox);

	model::Model* bucket2 = new model::Model("models/bucket/bucket2.obj");
	modelXCoord = 67, modelYCoord = 144;
	modelHeightInWorld = bucket2->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset);
	bucket2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(bucket2);
	hitBoxes.push_back(bucket2->hitBox);

	/* Paddock animals START */
	model::Model* cat = new model::Model("models/cat/cat.obj");
	modelXCoord = 85, modelYCoord = 145;
	modelHeightInWorld = cat->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 1.0f;
	cat->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(cat);
	hitBoxes.push_back(cat->hitBox);

	model::Model* cat2 = new model::Model("models/cat/cat.obj");
	modelXCoord = 75, modelYCoord = 160;
	modelHeightInWorld = cat2->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 1.0f;
	cat2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(cat2);
	hitBoxes.push_back(cat2->hitBox);
	/* Paddock animals END */

	/*Lost cat START*/
	lostCat;
	model::Model* cat3 = new model::Model("models/cat/cat.obj");
	modelXCoord = 200, modelYCoord = 360;
	modelHeightInWorld = cat3->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 1.0f;
	cat3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(cat3);
	lostCat.push_back(cat3);
	hitBoxes.push_back(cat3->hitBox);
	/*Lost cat END*/

	/* Giraffe Paddock */
	model::Paddock* paddock = new model::Paddock(5, 7);
	paddockXCoord = 5, paddockYCoord = 120;
	paddock->MovePaddock(glm::vec2(paddockXCoord, paddockYCoord), terra, cameraOffsetX, cameraOffsetY, terraYOffset);
	paddocks.push_back(paddock);
	paddock->PushModels(models);
	paddock->PushHitBoxes(hitBoxes);

	model::Model* bucket3 = new model::Model("models/bucket/bucket2.obj");
	modelXCoord = 20, modelYCoord = 118;
	modelHeightInWorld = bucket3->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset);
	bucket3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(bucket3);
	hitBoxes.push_back(bucket3->hitBox);



	model::Model* bucket4 = new model::Model("models/bucket/bucket2.obj");
	modelXCoord = 23, modelYCoord = 118;
	modelHeightInWorld = bucket4->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset);
	bucket4->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(bucket4);
	hitBoxes.push_back(bucket4->hitBox);

	model::Model* trough2 = new model::Model("models/trough/watertrough.obj");
	modelXCoord = 44, modelYCoord = 132;
	modelHeightInWorld = trough2->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) + 0.5f;
	trough2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(trough2);
	hitBoxes.push_back(trough2->hitBox);

	/* Paddock animals START */
	model::Model* giraffe = new model::Model("models/giraffe/giraffe-split.obj");
	modelXCoord = 35, modelYCoord = 130;
	modelHeightInWorld = giraffe->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) + 0.5f;
	giraffe->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	giraffe->SetRotationAnimationLoop("Head_Plane.001", -0.5f, 0.5f, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
	models.push_back(giraffe);
	hitBoxes.push_back(giraffe->hitBox);



	model::Model* giraffe2 = new model::Model("models/giraffe/giraffe-split.obj");
	modelXCoord = 15, modelYCoord = 150;
	modelHeightInWorld = giraffe->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) + 0.5f;
	giraffe2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	giraffe2->SetRotationAnimationLoop("Head_Plane.001", -0.5f, 0.5f, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
	models.push_back(giraffe2);
	hitBoxes.push_back(giraffe2->hitBox);

	model::Model* giraffe3 = new model::Model("models/giraffe/giraffe-split.obj");
	modelXCoord = 20, modelYCoord = 170;
	modelHeightInWorld = giraffe->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) + 0.5f;
	giraffe3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	giraffe3->SetRotationAnimationLoop("Head_Plane.001", -0.5f, 0.5f, 0.01f, glm::vec3(0.0f, 1.0f, 0.0f));
	models.push_back(giraffe3);
	hitBoxes.push_back(giraffe3->hitBox);
	/* Paddock animals END */

	model::Paddock* paddock3 = new model::Paddock(1, 1);
	paddockXCoord = 110, paddockYCoord = 110;
	paddock3->MovePaddock(glm::vec2(paddockXCoord, paddockYCoord), terra, cameraOffsetX, cameraOffsetY, terraYOffset);
	paddocks.push_back(paddock3);
	paddock3->PushModels(models);
	paddock3->PushHitBoxes(hitBoxes);


	model::Paddock* paddock4 = new model::Paddock(4, 5);
	paddockXCoord = 70, paddockYCoord = 180;
	paddock4->MovePaddock(glm::vec2(paddockXCoord, paddockYCoord), terra, cameraOffsetX, cameraOffsetY, terraYOffset);
	paddocks.push_back(paddock4);
	paddock4->PushModels(models);
	paddock4->PushHitBoxes(hitBoxes);

	model::Model* pig1 = new model::Model("models/pig/pig.obj");
	modelXCoord = 80;
	modelYCoord = 190;
	modelHeightInWorld = pig1->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 2;
	pig1->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(pig1);
	hitBoxes.push_back(pig1->hitBox);

	model::Model* pig2 = new model::Model("models/pig/pig.obj");
	modelXCoord = 85;
	modelYCoord = 185;
	modelHeightInWorld = pig2->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 2;
	pig2->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(pig2);
	hitBoxes.push_back(pig2->hitBox);

	model::Model* pig3 = new model::Model("models/pig/pig.obj");
	modelXCoord = 90;
	modelYCoord = 210;
	modelHeightInWorld = pig3->GetModelTerrainHeight(terra, modelXCoord, modelYCoord, cameraOffsetX, cameraOffsetY, terraYOffset) - 2;
	pig3->MoveTo(glm::vec3(modelXCoord, modelHeightInWorld, modelYCoord));
	models.push_back(pig3);
	hitBoxes.push_back(pig3->hitBox);

	//--------------
	// CREATE SKYBOX
	//--------------
	skybox::Skybox skybox = skybox::Skybox();
	skybox.getInt();

	// Init before the main loop
	float last_frame = glfwGetTime();
	float current_frame = 0.0f;
	float delta_time = 0.0f;
	//Set a background color
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	// Sounds
	cat->playSound("audio/cat-purring.wav", true, 0.2);
	terra.playSound("audio/meadow-birds.wav");

	// Main Loop
	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		audio::setListener(camera.get_position());
		camSource.setPosition(camera.get_position());
		/* PROCESS INPUT */
		current_frame = glfwGetTime();
		delta_time = current_frame - last_frame;
		last_frame = current_frame;

		// find the current rough terrain height at the camera position
		int cameraX = (int)camera.get_position().x + cameraOffsetX;
		int cameraY = (int)camera.get_position().z + cameraOffsetY;
		float terrainHeight = terra.getHeightAt(cameraX, cameraY) + terraYOffset + 5.0f; // using the offset down 20.0f units and adding some height for the camera
		process_input(window, delta_time, camera, terrainHeight, terra);

		glm::mat4 Hvw = camera.get_view_transform(); //view
		glm::mat4 Hcv = camera.get_clip_transform(); //projection


		//------------------------------------------
		// RENDER REFLECTION AND REFRACTION TEXTURES
		//------------------------------------------
		// If camera is above the water, do reflection and refraction as you would expect
		if (camera.get_position().y > water.getHeight() - 0.5)
		{
			// Allow clipping
			glEnable(GL_CLIP_DISTANCE0);

			// Bind the reflection frame buffer
			fbos.bindReflectionFrameBuffer();

			// Move the camera
			float distance = 2 * (camera.get_position().y - water.getHeight());
			camera.move_y_position(-distance);
			camera.invert_pitch();

			// Render the scene
			render(terra, camera, models, skybox, modelShader, glm::vec4(0, 1, 0, -water.getHeight()), SLmodels, streetLightShader);

			// Move the camera back
			camera.move_y_position(distance);
			camera.invert_pitch();

			// Bind the refraction frame buffer
			fbos.bindRefractionFrameBuffer();

			// Render the scene
			render(terra, camera, models, skybox, modelShader, glm::vec4(0, -1, 0, water.getHeight()), SLmodels, streetLightShader);
		}
		// If the camera is below the water, dont need reflection only refraction
		else
		{
			// Allow clipping
			glEnable(GL_CLIP_DISTANCE0);

			// Bind the reflection frame buffer
			fbos.bindReflectionFrameBuffer();

			// Render the scene, don't bother changing since this is refraction
			render(terra, camera, models, skybox, modelShader, glm::vec4(0, 1, 0, -water.getHeight()), SLmodels, streetLightShader);

			// Bind the refraction frame buffer
			fbos.bindRefractionFrameBuffer();
			// Render the scene
			render(terra, camera, models, skybox, modelShader, glm::vec4(0, -1, 0, -water.getHeight()), SLmodels, streetLightShader);
		}

		// Unbind the frame buffer before rendering the scene
		fbos.unbindCurrentFrameBuffer(SCREEN_WIDTH, SCREEN_HEIGHT);

		//-----------------
		// RENDER THE SCENE
		//-----------------
		// Render terrain, skybox and models
		render(terra, camera, models, skybox, modelShader, glm::vec4(0, 0, 0, 0), SLmodels, streetLightShader);
		// TODO: Send in a light when lights are done
		// Render water
		glEnable(GL_CLIP_DISTANCE0);
		water.draw(camera.get_view_transform(), camera.get_clip_transform(), camera.get_position(),
			glfwGetTime(), glm::vec3(0.0, 50, 0.0), glm::vec3(1.0, 1.0, 1.0), (camera.get_position().y > water.getHeight() - 0.5), camera.get_view_direction());
		glDisable(GL_CLIP_DISTANCE0);
		//Swap buffers
		glfwSwapBuffers(window);
		//Get and organize events, like keyboard and mouse input, window resizing, etc...
		glfwPollEvents();

	} // Check if the ESC key had been pressed or if the window had been closed
	while (!glfwWindowShouldClose(window));

	// Cleanup (delete buffers etc)
	terra.cleanup();
	fbos.cleanup();
	camSource.cleanup();
	alDeleteBuffers(1, &mainMusic);

	// Terminate OpenAL
	alcDestroyContext(context);
	alcCloseDevice(device);

	// Close OpenGL window and terminate GLFW
	glfwDestroyWindow(window);
	// Finalize and clean up GLFW
	glfwTerminate();

	exit(EXIT_SUCCESS);
}
