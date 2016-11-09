/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 45 - Platformer
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
using namespace std;

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define LEVEL_HEIGHT 24
#define LEVEL_WIDTH 150
#define SPRITE_COUNT_X 24
#define SPRITE_COUNT_Y 8
//#define tile_size 16

SDL_Window* displayWindow;

enum GameState {START_SCREEN, LEVEL_ONE};
int state;

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
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	SDL_FreeSurface(testTexture);
	return textureID;
}

class Vector3 {
public:
	Vector3() :x(0.0f), y(0.0f), z(0.0f){}
	Vector3(float initX, float initY, float initZ) : 
		x(initX), y(initY), z(initZ){}

	float x;
	float y;
	float z;
};

//class Entity to give all objects of the game a space to live in the program
class Entity{
public:
	Entity(SheetSprite iSprite, Vector3 iPosition, Vector3 iVelocity, Vector3 iSize) : 
		sprite(iSprite), position(iPosition), velocity(iVelocity), size(iSize), alive(true){}
	void Draw(ShaderProgram *program){
		sprite.Draw(program);
	}

	Vector3 position;
	Vector3 velocity;
	Vector3 size;
	float rotation;
	SheetSprite sprite;
	bool alive;
};

class shipEntity : public Entity{
public:
	shipEntity(SheetSprite iSprite, Vector3 iPosition, Vector3 iVelocity, Vector3 iSize, int iLives) :
		Entity(iSprite, iPosition, iVelocity, iSize), lives(iLives){}		//copy constructor

	int lives;
};

class bulletEntity : public Entity{		//bulletEntity derived class from Entity
public:
	bulletEntity(SheetSprite iSprite, Vector3 iPosition, Vector3 iVelocity, Vector3 iSize) : 
		Entity(iSprite, iPosition, iVelocity, iSize){}		//copy constructor
	float timeAlive = 0.0f;
};

//loads textures for building

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

//detects collision via the basic box-to-box algorithm, return true if detected, false otherwise
bool detectCollision(const Entity *a, const Entity *b){
			//if a's bottom is higher than b's top, no collision
	if ((a->position.y - (a->size.y / 2)) > (b->position.y + (b->size.y / 2)))
		return false;
			//if a's top if lower than b's bottom, no collision
	else if ((a->position.y + (a->size.y / 2)) > (b->position.y - (b->size.y / 2)))
		return false;
			//if a's left is larger than b's right, no collision
	else if ((a->position.x - (a->size.x / 2)) > (b->position.x + (b->size.x / 2)))
		return false;
			//if a's right is smaller than b's left, no collision
	else if ((a->position.x + (a->size.x / 2)) < (b->position.x - (b->size.x / 2)))
		return false;
	else
		return true;
}

void genWorldFromTile(ShaderProgram *program, GLuint tilesheet, Matrix *modelMatrix, unsigned char **levelData){
	float tile_size = 16;
	vector<float> vertexData;
	vector<float> texCoordData;
	for (int y = 0; y < LEVEL_HEIGHT; y++){
		for (int x = 0; x < LEVEL_WIDTH; x++){
			float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
			float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

			float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
			float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

			vertexData.insert(vertexData.end(), {

				tile_size * x, -tile_size * y,
				tile_size * x, (-tile_size * y) - tile_size,
				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,

				tile_size * x, -tile_size * y,
				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
				(tile_size * x) + tile_size, -tile_size * y
			});

			texCoordData.insert(texCoordData.end(), {
				u, v,
				u, v + (spriteHeight),
				u + spriteWidth, v + (spriteHeight),

				u, v,
				u + spriteWidth, v + (spriteHeight),
				u + spriteWidth, v
			});
		}
	}
	glBindTexture(GL_TEXTURE_2D, tilesheet);
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, &vertexData);
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, &texCoordData);
	glEnableVertexAttribArray(program->texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 600);

}

bool readLayerData(std::ifstream &stream, unsigned char **levelData) {
	string line;
	while (getline(stream, line)) {
		if (line == "") { break; }
		istringstream sStream(line);
		string key, value;
		getline(sStream, key, '=');
		getline(sStream, value);
		if (key == "data") {
			for (int y = 0; y < LEVEL_HEIGHT; y++) {
				getline(stream, line);
				istringstream lineStream(line);
				string tile;
				for (int x = 0; x < LEVEL_WIDTH; x++) {
					getline(lineStream, tile, ',');
					unsigned char val = (unsigned char)atoi(tile.c_str());
					if (val > 0) {
						// be careful, the tiles in this format are indexed from 1 not 0
						levelData[y][x] = val - 1;
					}
					else {
						levelData[y][x] = 0;
					}
				}
			}
		}
	}
	return true;
}


int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif

	SDL_Event event;
	bool done = false;

	//setting default color
	glClearColor(0.0f, 0.525f, 0.988f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640, 360);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint background = LoadTexture("Space Background.png");
	GLuint sprites = LoadTexture("sprites.png");
	GLuint tilesheet = LoadTexture("tilesheet.png");
	int font = LoadTexture("font.png");
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

	//define entity for the player ship
	Vector3 initializer;  //creates 0,0,0 vector3 for initialization
	SheetSprite blueShip(sprites, 0.0f, 0.0f, 422.0f / 1024.0f, 372.0f / 512.0f, 0.5f);	 //initializing textures from the spritesheet
	shipEntity player(blueShip, Vector3(0.0f, -1.6f, 0.0f), initializer, initializer, 3);
	player.size.x = blueShip.size * (blueShip.width / blueShip.height);
	player.size.y = blueShip.size;

	//vector to hold all enemy entities
	std::vector<shipEntity> enemies;
	SheetSprite enemySprite(sprites, 424.0f/1024.0f, 0.0f, 324.0f / 1024.0f, 340.0f / 512.0f, 0.3f);
	for (size_t i = 0; i < 8; i++){
		shipEntity enemy(enemySprite, Vector3(0.0f, 0.8f, 0.0f), initializer, initializer, 1);  //initializer defined where player ship created
		enemy.size.x = enemy.sprite.size * (enemy.sprite.width / enemy.sprite.height);  // is this right? ***** times aspect
		enemy.size.y = enemy.sprite.size;  // is this right? *****
		enemies.push_back(enemy);
	}


	unsigned char *levelData[LEVEL_HEIGHT][LEVEL_WIDTH];
	ifstream stream("double-layer-test.txt");

	//genWorldFromTile(&program, tilesheet, &modelMatrix, &levelData);
//************************************************************************************************************************

	readLayerData(stream, *levelData);
	float tile_size = 16;
	vector<float> vertexData;
	vector<float> texCoordData;
	for (int y = 0; y < LEVEL_HEIGHT; y++){
		for (int x = 0; x < LEVEL_WIDTH; x++){
			float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
			float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;

			float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
			float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;

			vertexData.insert(vertexData.end(), {

				tile_size * x, -tile_size * y,
				tile_size * x, (-tile_size * y) - tile_size,
				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,

				tile_size * x, -tile_size * y,
				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
				(tile_size * x) + tile_size, -tile_size * y
			});

			texCoordData.insert(texCoordData.end(), {
				u, v,
				u, v + (spriteHeight),
				u + spriteWidth, v + (spriteHeight),

				u, v,
				u + spriteWidth, v + (spriteHeight),
				u + spriteWidth, v
			});
		}
	}


//***********************************************************************************

	string first_line = "drggf";
	string second_line = "fsdgdfg";
	//creating an object pool for the player's bullets


	while (!done) {
		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;


		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN){
				if (state == LEVEL_ONE && event.key.keysym.scancode == SDL_SCANCODE_SPACE){    //press space to shoot
					
				}
				else if (state == START_SCREEN && event.key.keysym.scancode == SDL_SCANCODE_SPACE)
					state = LEVEL_ONE;
			}
		}

		////for general time-keeping between frames
		//float ticks = (float)SDL_GetTicks() / 1000.0f;
		//std::cout << SDL_GetTicks() << std::endl;
		//float elapsed = ticks - lastFrameTicks;
		//lastFrameTicks = ticks;
		//timeCount += elapsed;

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		if (state == LEVEL_ONE && keys[SDL_SCANCODE_LEFT]){
			
		}
		if (state == LEVEL_ONE && keys[SDL_SCANCODE_RIGHT]){
			
		}

		glClear(GL_COLOR_BUFFER_BIT);


		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);




		//*****************************

		glBindTexture(GL_TEXTURE_2D, tilesheet);
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, &vertexData);
		glEnableVertexAttribArray(program.positionAttribute);

		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, &texCoordData);
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 600);


		//*****************************





		//drawing the background image
		glBindTexture(GL_TEXTURE_2D, background);
		modelMatrix.identity();
		modelMatrix.Scale(8.0f, 4.5f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		//drawing the start screen text, if gamestate is set to it
		if (state == START_SCREEN){
			modelMatrix.identity();
			modelMatrix.Translate(-2.7f, 0.5f, 0.0f);
			drawText(&program, font, first_line, 0.25f, 0.03f, &modelMatrix);
			modelMatrix.identity();
			modelMatrix.Translate(-3.05f, 0.0f, 0.0f);
			drawText(&program, font, second_line, 0.18f, 0.0f, &modelMatrix);
		}

		//drawing the game level, if the gamestate is set to it
		else {
			//draw the score in the bottom-left corner
			modelMatrix.identity();
		
				
		}


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	SDL_Quit();
	return 0;
}