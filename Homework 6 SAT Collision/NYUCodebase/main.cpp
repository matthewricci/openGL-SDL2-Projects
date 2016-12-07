/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 6 - SAT Collision Demo
NOTES: 
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

#define LEVEL_HEIGHT 23     //number of rows of tiles (y value)
#define LEVEL_WIDTH 154     //number of columns of tiles (x value)
#define SPRITE_COUNT_X 16   //number of columns in the tilesheet
#define SPRITE_COUNT_Y 8    //number of rows in the tilesheet
#define TILE_SIZE 0.2
#define GRAVITY 28          //counteracts jumping velocity
#define FRICTION 10
// 60 FPS (1.0f/60.0f)
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define TRACKING false      //flag that sets viewMatrix player tracking

SDL_Window* displayWindow;

//global vars containing sounds used in-game, they are global vars so that class functions (e.g., Update) can access and play them
Mix_Chunk *keySound;
Mix_Chunk *jumpSound;
Mix_Music *music;

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

};

//converts two positions (e.g., player.x and player.y) into spots on the tile grid

//class Entity to give all objects of the game a space to live in the program
class Entity{
public:
	Entity(SheetSprite iSprite, float iWidth, float iHeight, float iX=0.0f, float iY=0.0f) : 
		sprite(iSprite), width(iWidth), height(iHeight), x(iX), y(iY),
	    velocity_x(0.0f), velocity_y(0.0f),
	    collidedTop(false), collidedBottom(false), collidedLeft(false), collidedRight(false){}
	void Draw(ShaderProgram *program){
		sprite.Draw(program);
	}

	void ResetFlags(){
		collidedTop = false;
		collidedBottom = false;
		collidedLeft = false;
		collidedRight = false;
	}

	void Update(float elapsed){
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
};


//detects collision via the basic box-to-box algorithm, return true if detected, false otherwise
bool detectCollisionTwoEntities(const Entity *a, const Entity *b){
	//if a's bottom is higher than b's top, no collision
	if ((a->y - (a->height / 2)) > (b->y + (b->height / 2)))
		return false;
	//if a's top if lower than b's bottom, no collision
	else if ((a->y + (a->height / 2)) < (b->y - (b->height / 2)))
		return false;
	//if a's left is larger than b's right, no collision
	else if ((a->x - (a->width / 2)) > (b->x + (b->width / 2)))
		return false;
	//if a's right is smaller than b's left, no collision
	else if ((a->x + (a->width / 2)) < (b->x - (b->width / 2)))
		return false;
	else
		return true;
}


//draws a string to screen given a font sheet and a string
void drawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing, Matrix *modelMatrix) {
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (int i = 0; i < text.size(); i++) {
		float texture_x = (float)(((int)text[i]) % 16) / 16.0f;
		float texture_y = (float)(((int)text[i]) / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
		});
	}
	glUseProgram(program->programID);

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);
	glBindTexture(GL_TEXTURE_2D, fontTexture);
	program->setModelMatrix(*modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, text.size() * 6);

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}




//updates the entire gamestate for all entities
void Update(float timeElapsed, Entity *white1, Entity *white2, Entity *white3){
	float fixedElapsed = timeElapsed;
	if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
		fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
	}
	while (fixedElapsed >= FIXED_TIMESTEP) {
		fixedElapsed -= FIXED_TIMESTEP;
		white1->Update(FIXED_TIMESTEP);
		white2->Update(FIXED_TIMESTEP);
		white3->Update(FIXED_TIMESTEP);
	}
	white1->Update(fixedElapsed);
	white2->Update(fixedElapsed);
	white3->Update(fixedElapsed);
}

//renders entire gamestate (should be called AFTER all Update() calls
void Render(Matrix *modelMatrix, Matrix *playerModelMatrix){

}

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


	GLuint whiteTexture = LoadTexture("white.png");
	SheetSprite whiteSprite(whiteTexture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	Entity white1(whiteSprite, (whiteSprite.size*(whiteSprite.width/whiteSprite.height)-0.6f), whiteSprite.size-0.2f, 0.0f, 0.0f);
	Entity white2(whiteSprite, (whiteSprite.size*(whiteSprite.width / whiteSprite.height) - 0.6f), whiteSprite.size - 0.0f, 0.0f);
	Entity white3(whiteSprite, (whiteSprite.size*(whiteSprite.width / whiteSprite.height) - 0.6f), whiteSprite.size - 0.0f, 0.0f);
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
		Update(elapsed, &white1, &white2, &white3);

		white1ModelMatrix.identity();
		white1ModelMatrix.Translate(white1.x, white1.y, 0.0f);
		program.setModelMatrix(white1ModelMatrix);
		white1.Draw(&program);
		white2ModelMatrix.identity();
		white2ModelMatrix.Translate(white2.x, white2.y, 0.0);
		program.setModelMatrix(white2ModelMatrix);
		white2.Draw(&program);
		white3ModelMatrix.identity();
		white3ModelMatrix.Translate(white3.x, white3.y, 0.0f);						//follow TSR
		white3ModelMatrix.Rotate(ticks * white3.rotation * (3.1415926 / 180.0));
		program.setModelMatrix(white3ModelMatrix);
		white3.Draw(&program);
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	Mix_FreeChunk(keySound);
	Mix_FreeChunk(jumpSound);
	Mix_FreeMusic(music);
	SDL_Quit();
	return 0;
}