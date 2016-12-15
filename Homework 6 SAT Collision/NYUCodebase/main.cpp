/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 6 - SAT Collision Demo
NOTES: This demo has three objects, white1 white2 white3, all from the same sprite, that are being scaled/rotated/translated at different rates.
The SAT algorithm checks collision for each set of edges for each object on each run of the Update() function.
Explaining the circles in the corners: To test my collision, I have the program render four circles, one at each world-space coordinate for an object, to visualize how the collision is working.
Undefined behavior:
	Through your help and suggestions after class, I was able to update my algorithm so that collision works well with both scaled and rotated objects.
	I also took your suggestion of cleaning up my game loop by placing messy chunks of code into functions (created a Render() and a highlightCorners() function.
	I still, however, can't figure out why the objects collide too soon on the left and right sides. I've tried a few different things, but ultimately
	can't afford to spend any more time debugging this without causing problems with my final project.
*/

#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <vector>
#include "Matrix.h"
#include "ShaderProgram.h"
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <SDL_mixer.h>
#include <algorithm>	//needed for SAT Collision detection

using namespace std;

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

// 60 FPS (1.0f/60.0f)
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6

SDL_Window* displayWindow;

//class SheetSprite for mapping and storing sprites from a sheet
class SheetSprite {
public:
	SheetSprite(unsigned int initTextureID, float initU, float initV, float initWidth, float initHeight, float initSize) :
		textureID(initTextureID), u(initU), v(initV), width(initWidth), height(initHeight), size(initSize){}
	void Draw(ShaderProgram *program){
		glBindTexture(GL_TEXTURE_2D, textureID);
		
		GLfloat texCoords[] = {
			u, v + height,
			u + width, v,
			u, v,
			u + width, v,
			u, v + height,
			u + width, v + height
		};
		float aspect = width / height;
		float vertices[] = {
			-0.5f * size * aspect, -0.5f * size,
			0.5f * size * aspect, 0.5f * size,
			-0.5f * size * aspect, 0.5f * size,
			0.5f * size * aspect, 0.5f * size,
			-0.5f * size * aspect, -0.5f * size,
			0.5f * size * aspect, -0.5f * size };

		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);

		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

GLuint LoadTexture(const char *image_path){
	SDL_Surface *testTexture = IMG_Load(image_path);
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, testTexture->w, testTexture->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, testTexture->pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	SDL_FreeSurface(testTexture);
	return textureID;
}

//LinEar inteRPolation - function gradually brings X acceleration to 0
float lerp(float v0, float v1, float t) {
	return (1.0 - t)*v0 + t*v1;
}

class Vector{
public:
	Vector(float iX=0.0f, float iY=0.0f, float iZ=0.0f) : x(iX), y(iY), z(iZ){}

	//uses pythagorean theorem to find the length of a vector
	float length(){
		return sqrt(x*x + y*y);
	}
	void normalize(){
		x /= length();
		y /= length();
	}
	float x;
	float y;
	float z;
	float normalizedX, normalizedY, normalizedZ;

};

bool testSATSeparationForEdge(float edgeX, float edgeY, const std::vector<Vector> &points1, const std::vector<Vector> &points2) {
	float normalX = -edgeY;
	float normalY = edgeX;
	float len = sqrtf(normalX*normalX + normalY*normalY);
	normalX /= len;
	normalY /= len;

	std::vector<float> e1Projected;
	std::vector<float> e2Projected;

	for (int i = 0; i < points1.size(); i++) {
		e1Projected.push_back(points1[i].x * normalX + points1[i].y * normalY);
	}
	for (int i = 0; i < points2.size(); i++) {
		e2Projected.push_back(points2[i].x * normalX + points2[i].y * normalY);
	}

	std::sort(e1Projected.begin(), e1Projected.end());
	std::sort(e2Projected.begin(), e2Projected.end());

	float e1Min = e1Projected[0];
	float e1Max = e1Projected[e1Projected.size() - 1];
	float e2Min = e2Projected[0];
	float e2Max = e2Projected[e2Projected.size() - 1];
	float e1Width = fabs(e1Max - e1Min);
	float e2Width = fabs(e2Max - e2Min);
	float e1Center = e1Min + (e1Width / 2.0);
	float e2Center = e2Min + (e2Width / 2.0);
	float dist = fabs(e1Center - e2Center);
	float p = dist - ((e1Width + e2Width) / 2.0);

	if (p < 0) {
		return true;
	}
	return false;
}

bool checkSATCollision(const std::vector<Vector> &e1Points, const std::vector<Vector> &e2Points) {
	for (int i = 0; i < e1Points.size(); i++) {
		float edgeX, edgeY;

		if (i == e1Points.size() - 1) {
			edgeX = e1Points[0].x - e1Points[i].x;
			edgeY = e1Points[0].y - e1Points[i].y;
		}
		else {
			edgeX = e1Points[i + 1].x - e1Points[i].x;
			edgeY = e1Points[i + 1].y - e1Points[i].y;
		}

		bool result = testSATSeparationForEdge(edgeX, edgeY, e1Points, e2Points);
		if (!result) {
			return false;
		}
	}
	for (int i = 0; i < e2Points.size(); i++) {
		float edgeX, edgeY;

		if (i == e2Points.size() - 1) {
			edgeX = e2Points[0].x - e2Points[i].x;
			edgeY = e2Points[0].y - e2Points[i].y;
		}
		else {
			edgeX = e2Points[i + 1].x - e2Points[i].x;
			edgeY = e2Points[i + 1].y - e2Points[i].y;
		}
		bool result = testSATSeparationForEdge(edgeX, edgeY, e1Points, e2Points);
		if (!result) {
			return false;
		}
	}
	return true;
}

Vector convertToWorldCoords(const Matrix &transformations, Vector objCoords){
	Vector worldCoords;
	worldCoords.x = ((transformations.m[0][0] * objCoords.x) + (transformations.m[1][0] * objCoords.y) + (transformations.m[3][0]));
	worldCoords.y = ((transformations.m[0][1] * objCoords.x) + (transformations.m[1][1] * objCoords.y) + (transformations.m[3][1]));
	worldCoords.z = 0.0;
	return worldCoords;
}

//converts two positions (e.g., player.x and player.y) into spots on the tile grid

//class Entity to give all objects of the game a space to live in the program
class Entity{
public:
	Entity(SheetSprite iSprite, float iWidth, float iHeight, float iX=0.0f, float iY=0.0f) : 
		sprite(iSprite), width(iWidth), height(iHeight), x(iX), y(iY),
	    velocity_x(0.0f), velocity_y(0.0f),
	    collidedTop(false), collidedBottom(false), collidedLeft(false), collidedRight(false)
	{
		Vector topLeftWorld, topRightWorld, bottomLeftWorld, bottomRightWorld;
		epoints.push_back(topLeftWorld);
		epoints.push_back(topRightWorld);
		epoints.push_back(bottomLeftWorld);
		epoints.push_back(bottomRightWorld);

	}
	void Draw(ShaderProgram *program){
		sprite.Draw(program);
	}

	void ResetFlags(){
		collidedTop = false;
		collidedBottom = false;
		collidedLeft = false;
		collidedRight = false;
	}

	void Update(float elapsed, const Matrix &transformations){
		x += velocity_x * elapsed;
		if (x > 3.25f){
			x = 3.23f;
			velocity_x *= -1;
		}
		else if (x < -3.25f){
			x = -3.23f;
			velocity_x *= -1;
		}
		y += velocity_y * elapsed;
		if (y > 1.98f){
			y = 1.96f;
			velocity_y *= -1;
		}
		else if (y < -1.98f){
			y = -1.96f;
			velocity_y *= -1;
		}

		//setting top-left corner
		topLeftObj.x = - (width / 2);
		topLeftObj.y = (height / 2);
		//setting top-right corner
		topRightObj.x = (width / 2);
		topRightObj.y = (height / 2);
		//setting bottom-left corner
		bottomLeftObj.x = - (width / 2);
		bottomLeftObj.y = - (height / 2);
		//setting bottom-right corner
		bottomRightObj.x = (width / 2);
		bottomRightObj.y = - (height / 2);

		//converting all points to world space
		epoints[0] = convertToWorldCoords(transformations, topLeftObj);
		epoints[1] = convertToWorldCoords(transformations, topRightObj);
		epoints[2] = convertToWorldCoords(transformations, bottomLeftObj);
		epoints[3] = convertToWorldCoords(transformations, bottomRightObj);
	}

	void highlightCorners(ShaderProgram *program, SheetSprite &test, Matrix &modelMatrix){
		for (size_t i = 0; i < epoints.size(); i++){
			modelMatrix.identity();
			modelMatrix.Translate(epoints[i].x, epoints[i].y, epoints[i].z);
			program->setModelMatrix(modelMatrix);
			test.Draw(program);
		}
	}

	SheetSprite sprite;
	float x;
	float y;
	float width;
	float height;
	float velocity_x;
	float velocity_y;
	float rotation = 100.0f;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;

	std::vector<Vector> epoints;

	//creating Vectors for Obj and World coords of each vertex
	Vector topLeftObj, topRightObj, bottomLeftObj, bottomRightObj;
};

//takes in pointers to the three white boxes and checks if any are colliding with any other via SAT collision
void checkCollisions(Entity *white1, Entity *white2, Entity *white3){
	int maxChecks;

	if (checkSATCollision(white1->epoints, white2->epoints)){
		white1->velocity_x *= -1;
		white1->velocity_y *= -1;
		white2->velocity_x *= -1;
		white2->velocity_y *= -1;
	}

	maxChecks = 10;

	while (checkSATCollision(white1->epoints, white2->epoints) && maxChecks > 0) {
		Vector responseVector = Vector(white1->x - white2->x, white1->y - white2->y);
		responseVector.normalize();
		white1->x += responseVector.x * 0.002;
		white1->y += responseVector.y * 0.002;

		white2->x -= responseVector.x * 0.002;
		white2->y -= responseVector.y * 0.002;
		maxChecks -= 1;
	}

	if (checkSATCollision(white1->epoints, white3->epoints)){
		white1->velocity_x *= -1;
		white1->velocity_y *= -1;
		white3->velocity_x *= -1;
		white3->velocity_y *= -1;
	}

	maxChecks = 10;

	while (checkSATCollision(white1->epoints, white3->epoints) && maxChecks > 0) {
		Vector responseVector = Vector(white1->x - white3->x, white1->y - white3->y);
		responseVector.normalize();
		white1->x += responseVector.x * 0.002;
		white1->y += responseVector.y * 0.002;

		white3->x -= responseVector.x * 0.002;
		white3->y -= responseVector.y * 0.002;
		maxChecks -= 1;
	}

	if (checkSATCollision(white2->epoints, white3->epoints)){
		white2->velocity_x *= -1;
		white2->velocity_y *= -1;
		white3->velocity_x *= -1;
		white3->velocity_y *= -1;
	}

	maxChecks = 10;

	while (checkSATCollision(white2->epoints, white3->epoints) && maxChecks > 0) {
		Vector responseVector = Vector(white2->x - white3->x, white2->y - white3->y);
		responseVector.normalize();
		white2->x += responseVector.x * 0.002;
		white2->y += responseVector.y * 0.002;

		white3->x -= responseVector.x * 0.002;
		white3->y -= responseVector.y * 0.002;
		maxChecks -= 1;
	}
}

//updates the entire gamestate for all entities
void Update(float timeElapsed, Entity *white1, Entity *white2, Entity *white3,
			const Matrix &white1Matrix, const Matrix &white2Matrix, const Matrix &white3Matrix){
	float fixedElapsed = timeElapsed;
	if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
		fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
	}

	int maxChecks = 10;
	//dividing elapsed time into fixed updates
	while (fixedElapsed >= FIXED_TIMESTEP) {
		fixedElapsed -= FIXED_TIMESTEP;
		white1->Update(FIXED_TIMESTEP, white1Matrix);
		white2->Update(FIXED_TIMESTEP, white2Matrix);
		white3->Update(FIXED_TIMESTEP, white3Matrix);

		checkCollisions(white1, white2, white3);
	}

	//one more round of updates to finish out left-over elapsed time
	white1->Update(fixedElapsed, white1Matrix);
	white2->Update(fixedElapsed, white2Matrix);
	white3->Update(fixedElapsed, white3Matrix);

	checkCollisions(white1, white2, white3);
}

//renders entire gamestate (should be called AFTER all Update() calls)
void Render(ShaderProgram *program, Entity &white1, Entity &white2, Entity &white3, 
			Matrix &white1ModelMatrix, Matrix &white2ModelMatrix, Matrix &white3ModelMatrix, Matrix &modelMatrix,
			float ticks, SheetSprite &test){
	//sets modelMatrix for white1 entity and draws it
	white1ModelMatrix.identity();
	white1ModelMatrix.Translate(white1.x, white1.y, 0.0f);
	//white1ModelMatrix.Scale(2.0f, 0.3f, 0.0f);
	white1ModelMatrix.Rotate(ticks * white3.rotation * (3.1415926 / 180.0));
	program->setModelMatrix(white1ModelMatrix);
	white1.Draw(program);

	//sets modelMatrix for white2 entity and draws it
	white2ModelMatrix.identity();
	white2ModelMatrix.Translate(white2.x, white2.y, 0.0f);
	white2ModelMatrix.Rotate(ticks * white2.rotation * (3.1415926 / 180.0));
	program->setModelMatrix(white2ModelMatrix);
	white2.Draw(program);

	//sets modelMatrix for white3 entity and draws it
	white3ModelMatrix.identity();
	white3ModelMatrix.Translate(white3.x, white3.y, 0.0f);  //follow TSR
	white3ModelMatrix.Scale(0.8f, 0.8f, 0.0f);
	white3ModelMatrix.Rotate(ticks * white3.rotation * (3.1415926 / 180.0));
	program->setModelMatrix(white3ModelMatrix);
	white3.Draw(program);

	//renders a red circle on each of the four corners of each entity to help debug the collision
	white1.highlightCorners(program, test, modelMatrix);
	white2.highlightCorners(program, test, modelMatrix);
	white3.highlightCorners(program, test, modelMatrix);
}




int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*1.5, 360*1.5, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif

	SDL_Event event;
	bool done = false;

	//setting default color
	glClearColor(0.0f, 0.025f, 0.088f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640*1.5, 360*1.5);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);

	GLuint testTexture = LoadTexture("circle.png");
	SheetSprite test(testTexture, 0.0f, 0.0f, 1.0f, 1.0f, 0.1f);
	GLuint white1Texture = LoadTexture("white1.png");
	GLuint white2Texture = LoadTexture("white2.png");
	GLuint white3Texture = LoadTexture("white3.png");


	SheetSprite white1Sprite(white1Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	SheetSprite white2Sprite(white2Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	SheetSprite white3Sprite(white3Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	Entity white1(white1Sprite, (white1Sprite.size*(white1Sprite.width / white1Sprite.height)), white1Sprite.size, -1.5f, -1.0f);
	Entity white2(white2Sprite, (white2Sprite.size*(white2Sprite.width / white2Sprite.height)), white2Sprite.size, 0.0f, 0.0f);
	Entity white3(white3Sprite, (white3Sprite.size*(white3Sprite.width / white3Sprite.height)), white3Sprite.size, 1.5f, 1.0f);
	white1.velocity_x = -1.0f;
	white1.velocity_y = 1.0f;
	white2.velocity_x = 2.0f;
	white2.velocity_y = 0.5f;
	white3.velocity_x = 0.8f;
	white3.velocity_y = -1.2f;
	
	Matrix projectionMatrix;																													
	Matrix modelMatrix;
	Matrix white1ModelMatrix;
	Matrix white2ModelMatrix;
	Matrix white3ModelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//initialized for keeping time during the game loop
	float lastFrameTicks = 0.0f;


	while (!done) {
		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;


		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}

		}

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		glClear(GL_COLOR_BUFFER_BIT);


		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };



		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);

		//updates the entire game (all three white boxes) every frame
		Update(elapsed, &white1, &white2, &white3, white1ModelMatrix, white2ModelMatrix, white3ModelMatrix);
		Render(&program, white1, white2, white3, white1ModelMatrix, white2ModelMatrix, white3ModelMatrix, modelMatrix, ticks, test);


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}

	SDL_Quit();
	return 0;
}