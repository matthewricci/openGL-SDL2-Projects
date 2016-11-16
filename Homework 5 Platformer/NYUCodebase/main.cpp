/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 5 - Platformer
NOTES: 
Platformer game that involves one player entity and a key entity, which can be collected and will unlock the gate at the end of the level.
A tilemap is used to generate the terrain in the level.

EXTRA CREDIT: Added background music on repeat, a jump sound, and a collect-key sound to this project

Unfinished features:
Spikes were supposed to hurt the player, but I ran out of time for the assignment before completing this feature.
I wanted the gate at the end to open up with a small animation (the tiles disappear one by one when you hit Q holding the key) but I couldn't get this working before submission time.
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

//converts two positions (e.g., player.x and player.y) into spots on the tile grid
void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {
	*gridX = (int)(worldX / TILE_SIZE);
	*gridY = (int)(-worldY / TILE_SIZE);
}

//class Entity to give all objects of the game a space to live in the program
class Entity{
public:
	Entity(SheetSprite iSprite, float iWidth, float iHeight, float iX=0.0f, float iY=0.0f) : 
		sprite(iSprite), width(iWidth), height(iHeight), x(iX), y(iY),
	    velocity_x(0.0f), velocity_y(0.0f), acceleration_x(0.0f), acceleration_y(0.0f),
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

	SheetSprite sprite;
	float x;
	float y;
	float width;
	float height;
	float velocity_x;
	float velocity_y;
	float acceleration_x;
	float acceleration_y;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
	bool jumping;      //used to test if player is currently jumping
};


class playerEnt : public Entity{
public:

	playerEnt(SheetSprite iSprite, float iWidth, float iHeight, int iLives, float iX = 0.0f, float iY = 0.0f) :
		Entity(iSprite, iWidth, iHeight, iX, iY), lives(iLives){}		

	void Update(float timeElapsed, const Uint8 *keys, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]);
	void ResetPlayer(Matrix *viewMatrix, float vm_x, float vm_y){
		acceleration_x = 0;
		acceleration_y = 0;
		velocity_x = 0;
		velocity_y = 0;
		x = 0.5f;
		y = -3.0f;
		viewMatrix->identity();
		viewMatrix->Translate(vm_x, vm_y, 0.0f);
	}

	int lives;
	int gridX, gridY;
	
};

class keyEnt : public Entity{
public:
	keyEnt(SheetSprite iSprite, float iWidth, float iHeight, float iX = 0.0f, float iY = 0.0f) :
		Entity(iSprite, iWidth, iHeight, iX, iY){}

	void Draw(ShaderProgram *program, Matrix *modelMatrix, float ticks, float playerX){
		modelMatrix->identity();
		if (!collected){
			modelMatrix->Translate(x, y, 0.0f);
			modelMatrix->Rotate(ticks * 300 * (3.1415926 / 180.0));
		}
		else{
			modelMatrix->Translate(playerX - 3.3f, -0.5f, 0.0f);
			modelMatrix->Rotate(-45 * (3.1415926 / 180.0));
		}
		program->setModelMatrix(*modelMatrix);
		sprite.Draw(program);
	}

	bool collected = false;
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

//performs collision checks on several different points all at once for a given entity
bool detectCollisionEntityAndTiles(playerEnt *ent, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
	float bottom = ent->y - (ent->height / 2);
	float top = ent->y + (ent->height / 2);       //get float values of all 4 sides of ent
	float left = ent->x - (ent->width / 2);
	float right = ent->x + (ent->width / 2);
	float upper = ent->y + (ent->width / 3);
	float lower = ent->y - (ent->width / 3);

	//check if bottom is colliding with a solid block
	worldToTileCoordinates(ent->x, bottom, &ent->gridX, &ent->gridY);  //convert to tile coords
	if (solidTiles[ent->gridY][ent->gridX] == true){                   //if tile is solid, there is a collision
		ent->y += ((-TILE_SIZE * ent->gridY) - (ent->y - (ent->height / 2)) - 0.0002f);  //move ent out of collision
		ent->acceleration_y = 0;
		ent->velocity_y = 0;
		ent->jumping = false;
	}

	//check if top is colliding with a solid block
	worldToTileCoordinates(ent->x, top, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->y -= ((ent->y + (ent->height / 2)) - ((-TILE_SIZE * ent->gridY) - TILE_SIZE) + 0.0002f);
		ent->acceleration_y = 0;
		ent->velocity_y = 0;
	}
	
	//check if left is colliding with a solid block
	worldToTileCoordinates(left, ent->y, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x += (((TILE_SIZE * ent->gridX) + TILE_SIZE) - (ent->x - ent->width / 2) - 0.0002f);
		//ent->acceleration_x = 0;
		//ent->velocity_x = 0;
	}

	//check if right is colliding with a solid block
	worldToTileCoordinates(right, ent->y, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true || solidTiles[ent->gridY+1][ent->gridX] == true){
		ent->x -= ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	//check if top-right corner is colliding with a solid block
	worldToTileCoordinates(right, top, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x -= ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	//check if bottom-right corner is colliding with a solid block ----- DOESN'T WORK, COLLIDES WITH GROUND AND SENDS YOU FLYING
	if (ent->jumping){
		worldToTileCoordinates(right, bottom, &ent->gridX, &ent->gridY);
		if (solidTiles[ent->gridY][ent->gridX] == true){
			ent->x -= ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.002f);
			ent->acceleration_x = 0;
			ent->velocity_x = 0;
		}
		worldToTileCoordinates(left, bottom, &ent->gridX, &ent->gridY);
		if (solidTiles[ent->gridY][ent->gridX] == true){
			ent->x += (((TILE_SIZE * ent->gridX) + TILE_SIZE) - (ent->x - ent->width / 2) - 0.0002f);
			ent->acceleration_x = 0;
			ent->velocity_x = 0;
		}
	}
	

	//check if upper-right corner is colliding with a solid block
	worldToTileCoordinates(right, upper, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x -= ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	//check if lower-right corner is colliding with a solid block
	worldToTileCoordinates(right, lower, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x -= ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	//check if upper-left corner is colliding with a solid block
	worldToTileCoordinates(right, upper, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x += ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	//check if lower-left corner is colliding with a solid block
	worldToTileCoordinates(right, lower, &ent->gridX, &ent->gridY);
	if (solidTiles[ent->gridY][ent->gridX] == true){
		ent->x += ((ent->x + (ent->width / 2)) - (TILE_SIZE * ent->gridX) + 0.0002f);
		ent->acceleration_x = 0;
		ent->velocity_x = 0;
	}

	return false;
}


void playerEnt::Update(float timeElapsed, const Uint8 *keys, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
	if (keys != NULL && jumping == false && keys[SDL_SCANCODE_SPACE]){
		velocity_y = 3.25f;
		jumping = true;
		Mix_PlayChannel(-1, jumpSound, 0);
	}
	if (x < 0.1f){
		x = 0.1f;
		acceleration_x = 0.0f;
		velocity_x = 0.0f;
	}
	acceleration_y -= GRAVITY * timeElapsed;
	velocity_y += acceleration_y * timeElapsed;
	y += velocity_y * timeElapsed;
	detectCollisionEntityAndTiles(this, solidTiles);
	if (keys != NULL && keys[SDL_SCANCODE_LEFT]){
			acceleration_x = -18.5f;
	}
	else if (keys != NULL && keys[SDL_SCANCODE_RIGHT]){
			acceleration_x = 18.5f;
	}
	else acceleration_x = 0;
	velocity_x += acceleration_x * timeElapsed;
	velocity_x = lerp(velocity_x, 0.0f, FRICTION * timeElapsed);
	x += velocity_x * timeElapsed;

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



//void genWorldFromTile(ShaderProgram *program, GLuint tilesheet, Matrix *modelMatrix, unsigned char **levelData){
//	float tile_size = 16;
//	vector<float> vertexData;
//	vector<float> texCoordData;
//	for (int y = 0; y < LEVEL_HEIGHT; y++){
//		for (int x = 0; x < LEVEL_WIDTH; x++){
//			float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float)SPRITE_COUNT_X;
//			float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float)SPRITE_COUNT_Y;
//
//			float spriteWidth = 1.0f / (float)SPRITE_COUNT_X;
//			float spriteHeight = 1.0f / (float)SPRITE_COUNT_Y;
//
//			vertexData.insert(vertexData.end(), {
//
//				tile_size * x, -tile_size * y,
//				tile_size * x, (-tile_size * y) - tile_size,
//				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
//
//				tile_size * x, -tile_size * y,
//				(tile_size * x) + tile_size, (-tile_size * y) - tile_size,
//				(tile_size * x) + tile_size, -tile_size * y
//			});
//
//			texCoordData.insert(texCoordData.end(), {
//				u, v,
//				u, v + (spriteHeight),
//				u + spriteWidth, v + (spriteHeight),
//
//				u, v,
//				u + spriteWidth, v + (spriteHeight),
//				u + spriteWidth, v
//			});
//		}
//	}
//	glBindTexture(GL_TEXTURE_2D, tilesheet);
//	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, &vertexData);
//	glEnableVertexAttribArray(program->positionAttribute);
//
//	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, &texCoordData);
//	glEnableVertexAttribArray(program->texCoordAttribute);
//
//	glDrawArrays(GL_TRIANGLES, 0, 600);
//
//}

//bool readLayerData(std::ifstream &stream, unsigned char **levelData) {
//	string line;
//	while (getline(stream, line)) {
//		if (line == "") { break; }
//		istringstream sStream(line);
//		string key, value;
//		getline(sStream, key, '=');
//		getline(sStream, value);
//		if (key == "data") {
//			for (int y = 0; y < LEVEL_HEIGHT; y++) {
//				getline(stream, line);
//				istringstream lineStream(line);
//				string tile;
//				for (int x = 0; x < LEVEL_WIDTH; x++) {
//					getline(lineStream, tile, ',');
//					unsigned char val = (unsigned char)atoi(tile.c_str());
//					if (val > 0) {
//						// be careful, the tiles in this format are indexed from 1 not 0
//						levelData[y][x] = val - 1;
//					}
//					else {
//						levelData[y][x] = 0;
//					}
//				}
//			}
//		}
//	}
//	return true;
//}

//updates the entire gamestate for all entities
void Update(float timeElapsed, const Uint8 *keys, playerEnt *player, bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
	float fixedElapsed = timeElapsed;
	if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
		fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
	}
	while (fixedElapsed >= FIXED_TIMESTEP) {
		fixedElapsed -= FIXED_TIMESTEP;
		player->Update(FIXED_TIMESTEP, keys, solidTiles);
	}
	player->Update(fixedElapsed, keys, solidTiles);
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
	glClearColor(0.0f, 0.525f, 0.988f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640*1.5, 360*1.5);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint tilesheet = LoadTexture("tilesheet.png");
	int font = LoadTexture("font.png");

	//for keeping viewMatrix constant and for following player movement  
	float vm_x = -3.55f;
	float vm_y = 2.3f;

	//array holding data on solid tile blocks, check against them to see if collision is present
	bool solidTiles[23][154];


	GLuint playerTexture = LoadTexture("player.png");
	SheetSprite playerSprite(playerTexture, 0.0f, 0.0f, 40.0f / 40.0f, 40.0f / 40.0f, 0.8f);
	playerEnt player(playerSprite, (playerSprite.size*(playerSprite.width/playerSprite.height)-0.6f), playerSprite.size-0.2f, 1, 0.5f, -3.0f);  //subtract -0.6f from width and -0.2f from height
																																				//to size the hitbox to the sprite properly
	SheetSprite keySprite(tilesheet, 6.0f*16.0f/256.0f, 5.0f*16.0f/128.0f, 16.0f/256.0f, 16.0f/128.0f, 0.4f);
	keyEnt key(keySprite, keySprite.size*(keySprite.width/keySprite.height), keySprite.size, 10.0f, -3.2f  );

	
	Matrix projectionMatrix;																													
	Matrix modelMatrix;
	Matrix playerModelMatrix;
	Matrix keyModelMatrix;
	Matrix fontModelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

	//strings to write
	string first_line = "Collect the key to open to gate ahead!";
	string second_line = "press Q with the key to pass through the gate.";

	//determining if player has collected key and hit 'Q' to open the gate
	bool gateOpened = false;
	int numTiles = 4;  //4 tiles need to be erased, one at a time, one every quarter of a second, and this helps keep count of when each is erased


	unsigned char levelData[LEVEL_HEIGHT][LEVEL_WIDTH];
	ifstream stream("only-arne-assets.txt");

	string line;
	for (size_t i = 0; i < 10; i++)     //the following 2 lines skip the header, since it is constant
		getline(stream, line);          //i felt it wasn't necessary to parse the header
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
						if (val == 2 || val == 4 || val == 5 || val == 8 || val == 17 || val == 19 || val == 21)
							solidTiles[y][x] = true;
					}
					else {
						levelData[y][x] = 0;
					}
				}
			}
		}
	}


	float tile_size = 0.2;
	vector<float> vertexData;
	vector<float> texCoordData;
	for (int y = 0; y < LEVEL_HEIGHT; y++){
		for (int x = 0; x < LEVEL_WIDTH; x++){

			if (levelData[y][x] > 0){
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
	}

	//initializes the ViewMatrix at the start of the game
	viewMatrix.identity();
	viewMatrix.Translate(vm_x, vm_y, 0.0f);
	program.setViewMatrix(viewMatrix);

	
	//SOUND EXTRA-CREDIT ADDITIONS
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);   //created audio buffer and sets a frequency and 2 channels for sound
	keySound = Mix_LoadWAV("key.wav");        //loads sounds
	jumpSound = Mix_LoadWAV("jump.wav");	  //loads sounds
	music = Mix_LoadMUS("ac_1200.mid");		  //loads music	Mix_PlayMusic(music, -1);				  //plays music file on infinite loop (-1)

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
				if (key.collected && event.key.keysym.scancode == SDL_SCANCODE_Q && player.x > 20.0f){
					gateOpened = true;
					Mix_PlayChannel(-1, keySound, 0);
				}
			}
		}

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		glClear(GL_COLOR_BUFFER_BIT);


		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };



		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);

		modelMatrix.identity();

		
		glBindTexture(GL_TEXTURE_2D, tilesheet);
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
		glEnableVertexAttribArray(program.positionAttribute);

		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);

		//update the viewMatrix to track
		if (player.x > -vm_x){
			viewMatrix.identity();
			viewMatrix.Translate(-player.x, vm_y, 0.0f);
			program.setViewMatrix(viewMatrix);
		}

		if (player.y < -4.0f)
			player.ResetPlayer(&viewMatrix, vm_x, vm_y);

		//there are two lines of text outputted to the level - the following renders them both in hardcoded positions on the map
		fontModelMatrix.identity();
		fontModelMatrix.Translate(0.4f, -1.75f, 0.0f);
		program.setModelMatrix(fontModelMatrix);
		drawText(&program, font, first_line, 0.1f, 0.005f, &fontModelMatrix);
		fontModelMatrix.Translate(20.0f, 0.0f, 0.0f);
		program.setModelMatrix(fontModelMatrix);
		drawText(&program, font, second_line, 0.1f, 0.005f, &fontModelMatrix);


		//updates the gamestate using the overarching Update() function and then draws the player based on these updates
		Update(elapsed, keys, &player, solidTiles);
		playerModelMatrix.identity();
		playerModelMatrix.Translate(player.x, player.y, 0.0f);
		program.setModelMatrix(playerModelMatrix);
		player.Draw(&program);

		if (detectCollisionTwoEntities(&player, &key)){
			if (!key.collected){
				key.collected = true;
				Mix_PlayChannel(-1, keySound, 0);
			}
		}

		key.Draw(&program, &keyModelMatrix, ticks, player.x );

		if (gateOpened){
			if (numTiles == 0) gateOpened = false;
			else{
				solidTiles[13+numTiles][128] = false;
				numTiles--;

			}
		}

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