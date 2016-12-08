/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 6 - SAT Collision Demo
NOTES: This demo has three objects, white1 white2 white3, all from the same sprite, that are being scaled/rotated/translated at different rates.
The SAT algorithm checks collision for each set of edges for each object on each run of the Update() function.
reversing the rotation during Update() makes things wonky
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
	float x;
	float y;
	float z = 0.0f;

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

Vector convertToWorldCoords(Matrix transformations, Vector objCoords){
	Vector worldCoords;
	worldCoords.x = ( (transformations.m[0][0] * objCoords.x) + (transformations.m[0][1] * objCoords.y) + (transformations.m[0][2] * objCoords.z) );
	worldCoords.y = ( (transformations.m[1][0] * objCoords.x) + (transformations.m[1][1] * objCoords.y) + (transformations.m[1][2] * objCoords.z) );
	worldCoords.z = ( (transformations.m[2][0] * objCoords.x) + (transformations.m[2][1] * objCoords.y) + (transformations.m[2][2] * objCoords.z) );
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

	void Update(float elapsed, Matrix transformations){
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
		topLeftObj.x = x - (width / 2);
		topLeftObj.y = y + (height / 2);
		//setting top-right corner
		topRightObj.x = x + (width / 2);
		topRightObj.y = y + (height / 2);
		//setting bottom-left corner
		bottomLeftObj.x = x - (width / 2);
		bottomLeftObj.y = y - (width / 2);
		//setting bottom-right corner
		bottomRightObj.x = x + (width / 2);
		bottomRightObj.y = y - (width / 2);

		//converting all points to world space
		epoints[0] = convertToWorldCoords(transformations, topLeftObj);
		epoints[1] = convertToWorldCoords(transformations, topRightObj);
		epoints[2] = convertToWorldCoords(transformations, bottomLeftObj);
		epoints[3] = convertToWorldCoords(transformations, bottomRightObj);

		//adjusting x and y of objects for the translations
		//x = epoints[0].x + (width / 2);
		//y = epoints[0].y - (height / 2);
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


//updates the entire gamestate for all entities
void Update(float timeElapsed, Entity *white1, Entity *white2, Entity *white3,
			Matrix white1Matrix, Matrix white2Matrix, Matrix white3Matrix){
	float fixedElapsed = timeElapsed;
	if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
		fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
	}
	//dividing elapsed time into fixed updates
	while (fixedElapsed >= FIXED_TIMESTEP) {
		fixedElapsed -= FIXED_TIMESTEP;
		white1->Update(FIXED_TIMESTEP, white1Matrix);
		white2->Update(FIXED_TIMESTEP, white2Matrix);
		white3->Update(FIXED_TIMESTEP, white3Matrix);
		if (checkSATCollision(white1->epoints, white2->epoints)){
			//if white1 is to the left of white 2
			if (white1->x < white2->x){
				white1->x -= 0.01f;
				white2->x += 0.01f;
			}
			else {
				white1->x += 0.01f;
				white2->x -= 0.01f;
			}
			//if white1 is below white2
			if (white1->y < white2->y){
				white1->y -= 0.01f;
				white2->y += 0.01f;
			}
			else {
				white1->y += 0.01f;
				white2->y -= 0.01f;
			}
			//reverse velocities
			white1->velocity_x *= -1;
			white1->velocity_y *= -1;
			white2->velocity_x *= -1;
			white2->velocity_y *= -1;
			//reverse direction of rotatation
			//white1->rotation *= -1;
			//	white2->rotation *= -1;
		}
		if (checkSATCollision(white1->epoints, white3->epoints)){
			//if white1 is to the left of white 3
			if (white1->x < white3->x){
				white1->x -= 0.01f;
				white3->x += 0.01f;
			}
			else {
				white1->x += 0.01f;
				white3->x -= 0.01f;
			}
			//if white1 is below white3
			if (white1->y < white3->y){
				white1->y -= 0.01f;
				white3->y += 0.01f;
			}
			else {
				white1->y += 0.01f;
				white3->y -= 0.01f;
			}
			//reverse velocities
			white1->velocity_x *= -1;
			white1->velocity_y *= -1;
			white3->velocity_x *= -1;
			white3->velocity_y *= -1;
			//reverse direction of rotatation
			//	white1->rotation *= -1;
			//white3->rotation *= -1;
		}
		if (checkSATCollision(white2->epoints, white3->epoints)){
			//if white2 is to the left of white3
			if (white2->x < white3->x){
				white2->x -= 0.01f;
				white3->x += 0.01f;
			}
			else {
				white2->x += 0.01f;
				white3->x -= 0.01f;
			}
			//if white2 is below white3
			if (white2->y < white3->y){
				white2->y -= 0.01f;
				white3->y += 0.01f;
			}
			else {
				white2->y += 0.01f;
				white3->y -= 0.01f;
			}
			//reverse velocities
			white2->velocity_x *= -1;
			white2->velocity_y *= -1;
			white3->velocity_x *= -1;
			white3->velocity_y *= -1;
			//reverse direction of rotatation
			//		white2->rotation *= -1;
			//white3->rotation *= -1;
		}
	}

	//one more round of updates to finish out left-over elapsed time
	white1->Update(fixedElapsed, white1Matrix);
	white2->Update(fixedElapsed, white2Matrix);
	white3->Update(fixedElapsed, white3Matrix);

	if (checkSATCollision(white1->epoints, white2->epoints)){
		//if white1 is to the left of white 2
		if (white1->x < white2->x){
			white1->x -= 0.01f;
			white2->x += 0.01f;
		}
		else {
			white1->x += 0.01f;
			white2->x -= 0.01f;
		}
		//if white1 is below white2
		if (white1->y < white2->y){
			white1->y -= 0.01f;
			white2->y += 0.01f;
		}
		else {
			white1->y += 0.01f;
			white2->y -= 0.01f;
		}
		//reverse velocities
		white1->velocity_x *= -1;
		white1->velocity_y *= -1;
		white2->velocity_x *= -1;
		white2->velocity_y *= -1;
		//reverse direction of rotatation
		//white1->rotation *= -1;
	//	white2->rotation *= -1;
	}
	if (checkSATCollision(white1->epoints, white3->epoints)){
		//if white1 is to the left of white 3
		if (white1->x < white3->x){
			white1->x -= 0.01f;
			white3->x += 0.01f;
		}
		else {
			white1->x += 0.01f;
			white3->x -= 0.01f;
		}
		//if white1 is below white3
		if (white1->y < white3->y){
			white1->y -= 0.01f;
			white3->y += 0.01f;
		}
		else {
			white1->y += 0.01f;
			white3->y -= 0.01f;
		}
		//reverse velocities
		white1->velocity_x *= -1;
		white1->velocity_y *= -1;
		white3->velocity_x *= -1;
		white3->velocity_y *= -1;
		//reverse direction of rotatation
	//	white1->rotation *= -1;
		//white3->rotation *= -1;
	}
	if (checkSATCollision(white2->epoints, white3->epoints)){
		//if white2 is to the left of white3
		if (white2->x < white3->x){
			white2->x -= 0.01f;
			white3->x += 0.01f;
		}
		else {
			white2->x += 0.01f;
			white3->x -= 0.01f;
		}
		//if white2 is below white3
		if (white2->y < white3->y){
			white2->y -= 0.01f;
			white3->y += 0.01f;
		}
		else {
			white2->y += 0.01f;
			white3->y -= 0.01f;
		}
		//reverse velocities
		white2->velocity_x *= -1;
		white2->velocity_y *= -1;
		white3->velocity_x *= -1;
		white3->velocity_y *= -1;
		//reverse direction of rotatation
//		white2->rotation *= -1;
		//white3->rotation *= -1;
	}
}

//renders entire gamestate (should be called AFTER all Update() calls
void Render(Matrix *modelMatrix, Matrix *playerModelMatrix){

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

	GLuint testTexture = LoadTexture("player.png");
	SheetSprite test(testTexture, 0.0f, 0.0f, 1.0f, 1.0f, 0.2f);
	GLuint white1Texture = LoadTexture("white1.png");
	GLuint white2Texture = LoadTexture("white2.png");
	GLuint white3Texture = LoadTexture("white3.png");


	SheetSprite white1Sprite(white1Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	SheetSprite white2Sprite(white2Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	SheetSprite white3Sprite(white3Texture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	Entity white1(white1Sprite, (white1Sprite.size*(white1Sprite.width / white1Sprite.height)), white1Sprite.size, 0.0f, 0.0f);
	Entity white2(white2Sprite, (white2Sprite.size*(white2Sprite.width / white2Sprite.height)), white2Sprite.size, 0.0f, 0.0f);
	Entity white3(white3Sprite, (white3Sprite.size*(white3Sprite.width / white3Sprite.height)), white3Sprite.size, 0.0f, 0.0f);
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

		white1ModelMatrix.identity();
		//white1ModelMatrix.Translate(white1.x, white1.y, 0.0f);
		//white1ModelMatrix.Rotate(ticks * white1.rotation * (3.1415926 / 180.0));
		white1ModelMatrix.Scale(2.0f, 0.3f, 1.0f);
		white1ModelMatrix.Translate(white1.x, white1.y, 0.0f);
		program.setModelMatrix(white1ModelMatrix);
		white1.Draw(&program);
		white2ModelMatrix.identity();
		white2ModelMatrix.Translate(white2.x, white2.y, 0.0);
		//white2ModelMatrix.Rotate(ticks * white2.rotation * (3.1415926 / 180.0));
		program.setModelMatrix(white2ModelMatrix);
		white2.Draw(&program);
		white3ModelMatrix.identity();
		white3ModelMatrix.Translate(white3.x, white3.y, 0.0f);						//follow TSR
		//white3ModelMatrix.Rotate(white3.rotation * (3.1415926 / 180.0));
		program.setModelMatrix(white3ModelMatrix);
		white3.Draw(&program);

		for (size_t i = 0; i < white1.epoints.size(); i++){
			modelMatrix.identity();
			modelMatrix.Translate(white1.epoints[i].x, white1.epoints[i].y, white1.epoints[i].z);
			program.setModelMatrix(modelMatrix);
			test.Draw(&program);
		}

		for (size_t j = 0; j < white3.epoints.size(); j++){
			modelMatrix.identity();
			modelMatrix.Translate(white3.epoints[j].x, white3.epoints[j].y, white3.epoints[j].z);
			program.setModelMatrix(modelMatrix);
			test.Draw(&program);
		}


		//testing functions
		//testing functions

		//white1ModelMatrix.identity();
		//white1ModelMatrix.Translate(-1.5f, 0.0f, 0.0f);
		//white1.x = -1.5f;
		//white1ModelMatrix.Rotate(45 * (3.1415926 / 180.0));
		//program.setModelMatrix(white1ModelMatrix);
		//white1.Draw(&program);
		//
		//white2ModelMatrix.identity();
		//white2ModelMatrix.Translate(1.5f, 0.0f, 0.0f);
		//white2.x = 1.5f;
		//white2ModelMatrix.Rotate(-45 * (3.1415926 / 180.0));
		//program.setModelMatrix(white2ModelMatrix);
		//white2.Draw(&program);

		//Vector white1Obj, white2Obj, white1World, white2World;
		//white1Obj.x = white1.x + (white1.width / 2);
		//white1Obj.y = white1.y - (white1.height / 2);
		//white1Obj.z = 0;
		//modelMatrix.identity();
		//modelMatrix.Translate(white1Obj.x, white1Obj.y, white1Obj.z);
		//program.setModelMatrix(modelMatrix);
		//test.Draw(&program);
		//white2Obj.x = white2.x - (white2.width / 2);
		//white2Obj.y = white2.y - (white2.height / 2);
		//white2Obj.z = 0;
		//modelMatrix.identity();
		//modelMatrix.Translate(white2Obj.x, white2Obj.y, white2Obj.z);
		//program.setModelMatrix(modelMatrix);
		//test.Draw(&program);
		//white1World = convertToWorldCoords(white1ModelMatrix, white1Obj);
		//white2World = convertToWorldCoords(white2ModelMatrix, white2Obj);

		//end of testers

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}

	SDL_Quit();
	return 0;
}