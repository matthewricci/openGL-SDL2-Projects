/*
NAME: Matthew Ricci
Assignment: CS3113 Final Project
NOTES:
use a library/linked list/something else to make a dialogue tree and store each dialogue's starting x position

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
#include <random>
using namespace std;

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define LEVEL_HEIGHT 20     //number of rows of tiles (y value)
#define LEVEL_WIDTH 25     //number of columns of tiles (x value)
#define SPRITE_COUNT_X 14   //number of columns in the tilesheet
#define SPRITE_COUNT_Y 16    //number of rows in the tilesheet
#define TILE_SIZE 0.2
//#define GRAVITY 5          //counteracts jumping velocity
#define FRICTION 12
// 60 FPS (1.0f/60.0f)
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define TRACKING false      //flag that sets viewMatrix player tracking

SDL_Window* displayWindow;

enum GameState { START_SCREEN, LEVEL_ONE, LEVEL_TWO, BATTLE };   //different states the game can be in
enum SpriteDirection { LEFT, RIGHT };							 //different directions the sprites can be looking
enum AIState {STANDBY, PURSUE, RETURN};

int state; //global var for the current game state the loop is in

//global vars containing sounds used in-game, they are global vars so that class functions (e.g., Update) can access and play them
Mix_Chunk *walkSound;
Mix_Chunk *jumpSound;
Mix_Music *music;

class Vector{
public:
	Vector(float x=0.0f, float y=0.0f) : x(x), y(y){}

	float x;
	float y;
};

class Particle {
public:
	Vector position;
	Vector velocity;
	float lifetime;
};

class ParticleEmitter {
public:
	ParticleEmitter(unsigned int particleCount, Vector &position, Vector &velocity, Vector velocityDeviation, Vector &gravity, float maxLifetime) : 
		position(position), velocity(velocity), velocityDeviation(velocityDeviation), gravity(gravity), maxLifetime(maxLifetime) {
		
		for (size_t i = 0; i < particleCount; i++){

		}

	}
	ParticleEmitter();
	~ParticleEmitter();


	void Update(float elapsed);
	void Render();

	void Draw(const ShaderProgram &program){
		std::vector<float> vertices;

		for (int i = 0; i < particles.size(); i++) {
			vertices.push_back(particles[i].position.x);
			vertices.push_back(particles[i].position.y);
		}

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices.data());
		glEnableVertexAttribArray(program.positionAttribute);
		glDrawArrays(GL_POINTS, 0, vertices.size() / 2);
	}

	Vector position;
	Vector gravity;
	Vector velocity;
	Vector velocityDeviation;
	float maxLifetime;

	std::vector<Particle> particles;
};

//class SheetSprite for mapping and storing sprites from a sheet
class SheetSprite {
public:
	SheetSprite(unsigned int initTextureID, float initU, float initV, float initWidth, float initHeight, float initSize) :
		textureID(initTextureID), u(initU), v(initV), width(initWidth), height(initHeight), size(initSize){}
	void Draw(ShaderProgram *program, SpriteDirection direction=RIGHT){
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
		if (direction == RIGHT){
			float vertices[] = {
				-0.5f * size * aspect, -0.5f * size,
				0.5f * size * aspect, 0.5f * size,
				-0.5f * size * aspect, 0.5f * size,
				0.5f * size * aspect, 0.5f * size,
				-0.5f * size * aspect, -0.5f * size,
				0.5f * size * aspect, -0.5f * size };
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
			glEnableVertexAttribArray(program->positionAttribute);
		}
		else {
			float verticesReflected[] = {
				-1 * -0.5f * size * aspect, -0.5f * size,
				-1 * 0.5f * size * aspect, 0.5f * size,
				-1 * -0.5f * size * aspect, 0.5f * size,
				-1 * 0.5f * size * aspect, 0.5f * size,
				-1 * -0.5f * size * aspect, -0.5f * size,
				-1 * 0.5f * size * aspect, -0.5f * size };
			glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, verticesReflected);
			glEnableVertexAttribArray(program->positionAttribute);
		}

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

GLuint LoadTextureNoAlpha(const char *image_path){
	SDL_Surface *testTexture = IMG_Load(image_path);
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, testTexture->w, testTexture->h, 0, GL_RGB, GL_UNSIGNED_BYTE, testTexture->pixels);
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
	Entity(GLuint iSpriteTexture, float iU, float iV, float iWidth, float iHeight, float iSize, int iNumSprites, int row = 0, float iX = 0.0f, float iY = 0.0f) :
		width(iWidth), height(iHeight), numSprites(iNumSprites), x(iX), y(iY),
		velocity_x(0.0f), velocity_y(0.0f), acceleration_x(0.0f), acceleration_y(0.0f),
		collidedTop(false), collidedBottom(false), collidedLeft(false), collidedRight(false), spriteTexture(iSpriteTexture){

		for (size_t i = 0; i < numSprites; i++){
			//SheetSprite newSprite(spriteTexture, (0.0f + width*i) / totalWidth, (height*row) / totalHeight, width / 256.0f, height / 128.0f, 1.4f);
			SheetSprite newSprite(spriteTexture, iU + iWidth*i, iV + (iHeight*(row-1)), iWidth, iHeight, iSize);  //row = row in sprite sheet, need row-1 because we wanna start at the end of the previous row
			sprites.push_back(newSprite);
		}
	
	}

	void Draw(ShaderProgram *program, SpriteDirection direction){
		sprites[spriteIndex].Draw(program, direction);
	}

	void ResetFlags(){
		collidedTop = false;
		collidedBottom = false;
		collidedLeft = false;
		collidedRight = false;
	}

	GLuint spriteTexture;
	std::vector<SheetSprite> sprites;	//CHILD CLASSES ARE RESPONSIBLE FOR SETTING THIS VECTOR! different entities may have different amount of sprites
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

	int gridX, gridY;

	int spriteIndex = 0;  //keeps track of what index of the sprites vector to draw
	int numSprites;   //how many sprites there are altogether for this entity
	float timeSinceLastSprite = 0.0f;   //used to slow down the walking animation for entities
};


class playerEnt : public Entity{
public:
	//width = width of one sprite, height = height of one sprite, totalWidth and totalHeight = width and height of the entire spritesheet, row = what row the sprites are on in the spritesheet, numSprites = how many SheetSprite objects to make (how many sprites for the entity)
	playerEnt(GLuint iSpriteTexture, float iU, float iV, float iWidth, float iHeight, float iSize, int numSprites = 1, int row = 0, float iX = 0.0f, float iY = 0.0f) :
	Entity(iSpriteTexture, iU, iV, iWidth, iHeight, iSize, numSprites, row, iX, iY){}

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

	SpriteDirection direction = RIGHT;
	int health = 100;
	int armor = 50;

};

class enemyEnt : public Entity{
public:
	enemyEnt(GLuint iSpriteTexture, float iU, float iV, float iWidth, float iHeight, float iSize, int numSprites = 1, int row = 0, float iX = 0.0f, float iY = 0.0f, float iVisionDistance = 1.35f) : 
	Entity(iSpriteTexture, iU, iV, iWidth, iHeight, iSize, numSprites, row, iX, iY), basePositionX(iX), basePositionY(iY), visionDistance(iVisionDistance){}

	bool proximityTest(float otherEntX, float otherEntY){
		//using Pythagorean theorem to define a ray test between this entity and another entity
		float ray = sqrt(abs(x - otherEntX) + abs(y - otherEntY));
		if (ray <= visionDistance){ return true; }
		else { return false; }
	}

	void Update(float timeElapsed, const float playerX, const float playerY, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]);

	GLuint spriteTexture;
	SpriteDirection direction = RIGHT;
	int gridX, gridY;
	float basePositionX;
	float basePositionY;
	float visionDistance;
	int health = 50;
	AIState currentState = STANDBY;
	bool alive = true;

};

//global var pointing to the enemy currently/most recently in battle with player
//enemyEnt *battleEnemy; this pointer causes serious breakpoint problems so just use size_t
size_t battleEnemy;

//class enemyEnt : public Entity{
//public:
//	enemyEnt(SheetSprite iSprite, float iWidth, float iHeight, int iLives, float iX = 0.0f, float iY = 0.0f) :
//		Entity(iSprite, iWidth, iHeight, iX, iY) {}
//};

//class keyEnt : public Entity{
//public:
//	keyEnt(SheetSprite iSprite, float iWidth, float iHeight, float iX = 0.0f, float iY = 0.0f) :
//		Entity(iSprite, iWidth, iHeight, iX, iY){}
//
//	void Draw(ShaderProgram *program, Matrix *modelMatrix, float ticks, float playerX){
//		modelMatrix->identity();
//		if (!collected){
//			modelMatrix->Translate(x, y, 0.0f);
//			modelMatrix->Rotate(ticks * 300 * (3.1415926 / 180.0));
//		}
//		else{
//			modelMatrix->Translate(playerX - 3.3f, -0.5f, 0.0f);
//			modelMatrix->Rotate(-45 * (3.1415926 / 180.0));
//		}
//		program->setModelMatrix(*modelMatrix);
//		sprite.Draw(program);
//	}
//
//	bool collected = false;
//};

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

void fixEntityCollision(Entity *a, Entity *b){
	//if a's bottom is lower than b's top AND a's top is higher than b's top, we need to move a's Y up and b's Y down

}

//bool newDetectCollisionTwoEntities(float timeElapsed, Entity *a, Entity *b){
//	// if a's top if higher than b's bottom, possible collision
//	if ((a->y + (a->height / 2)) > (b->y - (b->height / 2))){
//		if (a->x < b->x){ //if a is to the left of b
//			//if a's right side is past b's left side, we are colliding
//			if ((a->x + (a->width / 2)) >(b->x - (b->width / 2))){
//
//				return true;
//			}
//		}
//	}
//
//	//if a's bottom is lower than b's top, possible collision
//	else if ((a->y - (a->height / 2)) < (b->y + (b->height / 2))){
//
//	}
//}

//performs collision checks on several different points all at once for a given entity
bool detectCollisionEntityAndTiles(Entity *ent, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
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
	if (solidTiles[ent->gridY][ent->gridX] == true || solidTiles[ent->gridY + 1][ent->gridX] == true){
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
	if (keys != NULL && keys[SDL_SCANCODE_UP]){
		acceleration_y = 7.5f;
		timeSinceLastSprite += timeElapsed;
		if (timeSinceLastSprite > 0.1f){
			timeSinceLastSprite = 0.0f;
			spriteIndex++;
			if (spriteIndex % 2)	//only play the sound on every other sprite change
				Mix_PlayChannel(-1, walkSound, 0);
			if (spriteIndex >= numSprites)  //if the sprite index is greater than the total number of sprites
				spriteIndex %= numSprites;  //set it back to 0 using modulo
		}
	}
	else if (keys != NULL && keys[SDL_SCANCODE_DOWN]){
		acceleration_y = -7.5f;
		timeSinceLastSprite += timeElapsed;
		if (timeSinceLastSprite > 0.1f){
			timeSinceLastSprite = 0.0f;
			spriteIndex++;
			if (spriteIndex % 2)	//only play the sound on every other sprite change
				Mix_PlayChannel(-1, walkSound, 0);
			if (spriteIndex >= numSprites)  //if the sprite index is greater than the total number of sprites
				spriteIndex %= numSprites;  //set it back to 0 using modulo
		}
	}
	else acceleration_y = 0;

	velocity_y += acceleration_y * timeElapsed;
	velocity_y = lerp(velocity_y, 0.0f, FRICTION * timeElapsed);
	y += velocity_y * timeElapsed;
	detectCollisionEntityAndTiles(this, solidTiles);

	if (keys != NULL && keys[SDL_SCANCODE_LEFT]){
		direction = LEFT;
		acceleration_x = -10.5f;
		if (!keys[SDL_SCANCODE_UP] && !keys[SDL_SCANCODE_DOWN]){
			timeSinceLastSprite += timeElapsed;
			if (timeSinceLastSprite > 0.1f){
				timeSinceLastSprite = 0.0f;
				spriteIndex++;
				if (spriteIndex % 2)	//only play the sound on every other sprite change
					Mix_PlayChannel(-1, walkSound, 0);
				if (spriteIndex >= numSprites)  //if the sprite index is greater than the total number of sprites
					spriteIndex %= numSprites;  //set it back to 0 using modulo

			}
		}
	}
	else if (keys != NULL && keys[SDL_SCANCODE_RIGHT]){
		direction = RIGHT;
		acceleration_x = 10.5f;
		if (!keys[SDL_SCANCODE_UP] && !keys[SDL_SCANCODE_DOWN]){
			timeSinceLastSprite += timeElapsed;
			if (timeSinceLastSprite > 0.1f){
				timeSinceLastSprite = 0.0f;
				spriteIndex++;
				if (spriteIndex % 2)	//only play the sound on every other sprite change
					Mix_PlayChannel(-1, walkSound, 0);
				if (spriteIndex >= numSprites)  //if the sprite index is greater than the total number of sprites
					spriteIndex %= numSprites;  //set it back to 0 using modulo
			}
		}
	}
	else acceleration_x = 0;

	velocity_x += acceleration_x * timeElapsed;
	velocity_x = lerp(velocity_x, 0.0f, FRICTION * timeElapsed);
	x += velocity_x * timeElapsed;
	detectCollisionEntityAndTiles(this, solidTiles);

}

void enemyEnt::Update(float timeElapsed, float playerX, float playerY, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
	
	bool isInRange = proximityTest(playerX, playerY);

	//pair of if statements checking of the currentState variable needs changing
	if (currentState != PURSUE && isInRange){
		currentState = PURSUE;
	}
	else if (!isInRange){
		if (currentState != STANDBY && abs(x - basePositionX) < 0.05f && abs(y - basePositionY) < 0.05f){
			currentState = STANDBY;
		}
		else if (currentState != RETURN && abs(x - basePositionX) > 0.05f || abs(y - basePositionY) > 0.05f){
			currentState = RETURN;
		}
	}

	//once the variable has been changed/not changed, it is checked a second time to determine how to update position
	if (currentState == PURSUE){
		//y-axis first
		acceleration_y = 8.0f;
		velocity_y += acceleration_y * timeElapsed;
		velocity_y = lerp(velocity_y, 0.0f, FRICTION * timeElapsed);
		y += (playerY - y) * velocity_y * timeElapsed;
		detectCollisionEntityAndTiles(this, solidTiles);
		//then x-axis
		acceleration_x = 8.0f;
		velocity_x += acceleration_x * timeElapsed;
		velocity_x = lerp(velocity_x, 0.0f, FRICTION * timeElapsed);
		x += (playerX - x) * velocity_x * timeElapsed;
		detectCollisionEntityAndTiles(this, solidTiles);

		if (playerX - x < 0){
			direction = LEFT;
		}
		else direction = RIGHT;

	}
	else if ( currentState == RETURN){
		//y-axis first
		acceleration_y = 5.0f;
		velocity_y += acceleration_y * timeElapsed;
		velocity_y = lerp(velocity_y, 0.0f, FRICTION * timeElapsed);
		y += (basePositionY - y) * velocity_y * timeElapsed;
		detectCollisionEntityAndTiles(this, solidTiles);
		//then x-axis
		acceleration_x = 5.0f;
		velocity_x += acceleration_x * timeElapsed;
		velocity_x = lerp(velocity_x, 0.0f, FRICTION * timeElapsed);
		x += (basePositionX - x) * velocity_x * timeElapsed;
		detectCollisionEntityAndTiles(this, solidTiles);

		if (basePositionX - x < 0){
			direction = LEFT;
		}
		else direction = RIGHT;

	}
	else{  //currentState must be STANDBY
		acceleration_x = 0.0f;
		//velocity_x = lerp(velocity_x, 0.0f, FRICTION * timeElapsed);
		acceleration_y = 0.0f;
		//velocity_y = lerp(velocity_y, 0.0f, FRICTION * timeElapsed);
	}


}

//draws a string to screen given a font sheet and a string
void drawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing, Matrix *modelMatrix) {
	float texture_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;

//	int numChars = (int)(elapsed / slowness);  //elapsed == how long since the text first started printing, slowness == how fast each char should be printed
//	if (numChars > text.size())         //as "slowness" value decreases, chars are printed faster 
//		numChars = text.size();         //if numChars is greater than the amount of chars in string, just print the size of the string

	for (int i = 0; i < text.size(); i++) {
//	for (int i = 0; i < numChars-1; i++) {
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

void drawTextBox(ShaderProgram *program, GLuint textBoxTexture, GLuint fontTexture, Matrix &boxModelMatrix, Matrix &fontModelMatrix, std::string text, float size, float spacing, float x, float y){
	
	glBindTexture(GL_TEXTURE_2D, textBoxTexture);

	boxModelMatrix.identity();
	boxModelMatrix.Translate(x, y-1.5f, 0.0f);
	boxModelMatrix.Scale(7.3f, 1.0f, 1.0f);
	program->setModelMatrix(boxModelMatrix);

	float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program->positionAttribute);

	float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program->texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	fontModelMatrix.identity();
	fontModelMatrix.Translate(x-2.5f, y-1.4f, 0.0f);
	drawText(program, fontTexture, text, size, spacing, &fontModelMatrix);

	//regardless of the above text, this will always draw the hint string near the bottom-middle of the text box
	std::string hint = "- Press space to continue - ";
	fontModelMatrix.identity();
	fontModelMatrix.Translate(x - 1.4f, y - 1.7f, 0.0f);
	drawText(program, fontTexture, hint, 0.1, spacing, &fontModelMatrix);
}

//updates the entire gamestate for all entities
void Update(float timeElapsed, const Uint8 *keys, playerEnt *player, std::vector<enemyEnt> enemies, const bool solidTiles[LEVEL_HEIGHT][LEVEL_WIDTH]){
	float fixedElapsed = timeElapsed;
	if (fixedElapsed > FIXED_TIMESTEP * MAX_TIMESTEPS) {
		fixedElapsed = FIXED_TIMESTEP * MAX_TIMESTEPS;
	}
	while (fixedElapsed >= FIXED_TIMESTEP) {
		fixedElapsed -= FIXED_TIMESTEP;
		player->Update(FIXED_TIMESTEP, keys, solidTiles);
		for (size_t i = 0; i < enemies.size(); i++){
			enemies[i].Update(FIXED_TIMESTEP, player->x, player->y, solidTiles);
		}
	}
	player->Update(fixedElapsed, keys, solidTiles);
	for (size_t i = 0; i < enemies.size(); i++){
		enemies[i].Update(fixedElapsed, player->x, player->y, solidTiles);
		if (state != BATTLE && enemies[i].alive==true && detectCollisionTwoEntities(player, &enemies[i])){
			battleEnemy = i;
			state = BATTLE;
		}
	}
}

//renders entire gamestate (should be called AFTER all Update() calls
void Render(Matrix *modelMatrix, Matrix *playerModelMatrix){

}

void triggerFlag(bool &flag){
	if (flag){
		flag = false;
	}
	else{
		flag = true;
	}
}

void drawBackground(ShaderProgram *program, GLuint background, Matrix &modelMatrix, float x, float y){
	glBindTexture(GL_TEXTURE_2D, background);
	modelMatrix.identity();
	modelMatrix.Translate(x, y, 0.0f);
	modelMatrix.Scale(7.0f, 4.0f, 1.0f);
	program->setModelMatrix(modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640 * 1.5, 360 * 1.5, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif

	SDL_Event event;
	bool done = false;

	//initialize game to the start screen
	state = START_SCREEN;

	//setting default color
	//makes a light greyglClearColor(0.529f, 0.490f, 0.490f, 1.0f);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	//glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	//texture coordinates
	glViewport(0, 0, 640 * 1.5, 360 * 1.5);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	ShaderProgram program2(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	glUseProgram(program.programID);
	GLuint tilesheet = LoadTexture("walls1.png");
	GLuint font = LoadTexture("font.png");
	GLuint textBoxTexture = LoadTexture("textBox.png");
	GLuint white = LoadTexture("white.png");
	GLuint battleBackground = LoadTextureNoAlpha("battleBackground.png");
	//GLuint gauntTest = LoadTexture("gaunt.png");

	//for keeping viewMatrix constant and for following player movement  
	float vm_x = -3.55f;
	float vm_y = 2.3f;

	//array holding data on solid tile blocks, check against them to see if collision is present
	bool solidTiles[20][25];

	GLuint menuArtTexture = LoadTexture("AstronautMenu.png");
	SheetSprite menuArt(menuArtTexture, 0.0f, 0.0f, 1.0f, 1.0f, 2.8f);
	GLuint playerTexture = LoadTexture("characters_3.png");
	//SheetSprite playerSprite(playerTexture, 0.0f/256.0f, 64.0f/128.0f, 32.0f / 256.0f, 32.0f / 128.0f, 0.8f);
	//playerEnt player(playerTexture, 32.0f/250.0f, 63.0f, 128.0f,  0.5f, -3.55f);  //subtract -0.6f from width and -0.2f from height
	playerEnt player(playerTexture, 0.0f/256.0f, 0.0f/128.0f, 32.0f/256.0f, 32.0f/128.0f, 0.8f, 8, 3, 0.5f, -3.55f);
	
	std::vector<enemyEnt> enemies;
		enemies.push_back(enemyEnt(playerTexture, 0.0f / 256.0f, 0.0f / 128.0f, 32.0f / 256.0f, 32.0f / 128.0f, 0.8f, 8, 3, 0.5f, -2.55f));
		enemies.push_back(enemyEnt(playerTexture, 0.0f / 256.0f, 0.0f / 128.0f, 32.0f / 256.0f, 32.0f / 128.0f, 0.8f, 8, 3, 1.5f, -1.55f));

	//DELETE?
	SheetSprite keySprite(tilesheet, 6.0f*16.0f / 256.0f, 5.0f*16.0f / 128.0f, 16.0f / 256.0f, 16.0f / 128.0f, 0.4f);


	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix playerModelMatrix;
	Matrix enemyModelMatrix;
	Matrix boxModelMatrix;
	Matrix fontModelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

	//for writing text via a scrolling animation
	int numChars = 0;
	float textElapsed;

	//strings to write
	string first_line = "Title card! This is a menu.";
	string second_line = "press Q with the key to pass through the gate.";

	//determining if player has collected key and hit 'Q' to open the gate
	bool gateOpened = false;
	int numTiles = 4;  //4 tiles need to be erased, one at a time, one every quarter of a second, and this helps keep count of when each is erased


	unsigned char levelData[LEVEL_HEIGHT][LEVEL_WIDTH];
	ifstream stream("gauntletTest.txt");

	//initializes the ViewMatrix at the start of the game
	viewMatrix.identity();
	viewMatrix.Translate(vm_x, vm_y, 0.0f);
	program.setViewMatrix(viewMatrix);


	//SOUND EXTRA-CREDIT ADDITIONS
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);   //created audio buffer and sets a frequency and 2 channels for sound
	walkSound = Mix_LoadWAV("walking3.wav");        //loads sounds
//	jumpSound = Mix_LoadWAV("jump.wav");	  //loads sounds
//	music = Mix_LoadMUS("backgroundMusic.mp3");		  //loads music
//	Mix_PlayMusic(music, -1);				  //plays music file on infinite loop (-1)


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


	while (!done) {
		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;


		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (state == START_SCREEN && event.type == SDL_KEYDOWN){
				if (event.key.keysym.scancode == SDL_SCANCODE_SPACE)
					state = LEVEL_ONE;
			}
			else if (state != BATTLE && event.type == SDL_KEYDOWN){
				if (event.key.keysym.scancode == SDL_SCANCODE_Q)
					state = BATTLE;
			}
			else if (state == BATTLE && event.type == SDL_KEYDOWN){
				if (event.key.keysym.scancode == SDL_SCANCODE_Q){
					state = LEVEL_ONE;
					enemies[battleEnemy].alive = false;
					}
			}
//			else if (event.type == SDL_KEYDOWN){
				//if (event.key.keysym.scancode == SDL_SCANCODE_UP){
				//	player.y += 100.0f;
				//}
				//else if (event.key.keysym.scancode == SDL_SCANCODE_DOWN){
				//	player.velocity_y -= 100.0f;
				//}
//			}
		}

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		glClear(GL_COLOR_BUFFER_BIT);

		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);

		modelMatrix.identity();

		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5, };

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		if (state == LEVEL_ONE){

			glBindTexture(GL_TEXTURE_2D, tilesheet);
			glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
			glEnableVertexAttribArray(program.positionAttribute);

			glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
			glEnableVertexAttribArray(program.texCoordAttribute);

			glDrawArrays(GL_TRIANGLES, 0, vertexData.size() / 2);


			//update the viewMatrix to track
				viewMatrix.identity();
				viewMatrix.Translate(-player.x, -player.y, 0.0f);
				program.setViewMatrix(viewMatrix);
			

			//if (player.y < -4.0f)
			//	player.ResetPlayer(&viewMatrix, vm_x, vm_y);

			//updates the gamestate using the overarching Update() function and then draws the player based on these updates
			Update(elapsed, keys, &player, enemies, solidTiles);
			playerModelMatrix.identity();
			playerModelMatrix.Translate(player.x, player.y, 0.0f);
			//playerModelMatrix.Scale(1.5f, 1.0f, 1.0f);				//remember to change this if you change sprites!!

			program.setModelMatrix(playerModelMatrix);

			player.Draw(&program, player.direction);

			for (size_t i = 0; i < enemies.size(); i++){
				if (enemies[i].currentState != PURSUE && enemies[i].proximityTest(player.x, player.y)){
					enemies[i].currentState = PURSUE;
				}
				else enemies[i].currentState = STANDBY;
				enemies[i].Update(elapsed, player.x, player.y, solidTiles);
				enemyModelMatrix.identity();
				enemyModelMatrix.Translate(enemies[i].x, enemies[i].y, 0.0f);
				enemyModelMatrix.Scale(1.5f, 1.0f, 1.0f);
				program.setModelMatrix(enemyModelMatrix);
				enemies[i].Draw(&program, enemies[i].direction);
				fontModelMatrix.identity();
				fontModelMatrix.Translate(enemies[i].x - 0.08f, enemies[i].y + 0.3f, 0.0f);
				program.setModelMatrix(fontModelMatrix);
				drawText(&program, font, std::to_string(enemies[i].health), 0.13f, 0.0f, &fontModelMatrix);
			}



			//player.spriteIndex++;

			//if (player.spriteIndex >= player.numSprites)  //if the sprite index is greater than the total number of sprites
			//	player.spriteIndex %= player.numSprites;  //set it back to 0 using modulo


			drawTextBox(&program, textBoxTexture, font, boxModelMatrix, fontModelMatrix, first_line, 0.15, 0.0, player.x, player.y);


		}
		else if (state == BATTLE){
			drawBackground(&program, battleBackground, modelMatrix, player.x, player.y);
			//battleEnemy->Draw(&program, RIGHT);
		}
		else{
			modelMatrix.identity();
			//modelMatrix.Scale(2.3f, 5.8f, 1.0f);
			//modelMatrix.Translate(2.65f, -0.25f, 0.0f);			
			//border on bottom side
			modelMatrix.Scale(2.3f, 5.3f, 1.0f);
			modelMatrix.Translate(2.7f, -0.25f, 0.0f);
			//border on right and bottom sides
			//modelMatrix.Scale(2.0f, 5.0f, 1.0f);
			//modelMatrix.Translate(3.0f, -0.25f, 0.0f);
			program.setModelMatrix(modelMatrix);
			glBindTexture(GL_TEXTURE_2D, white);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			fontModelMatrix.identity();
			fontModelMatrix.Translate(0.4f, -1.75f, 0.0f);
			glBindTexture(GL_TEXTURE_2D, textBoxTexture);
			program.setModelMatrix(fontModelMatrix);
			glDrawArrays(GL_TRIANGLES, 0, 6);
			drawText(&program, font, first_line, 0.1f, 0.0f, &fontModelMatrix);
			//playerModelMatrix.identity();
			//playerModelMatrix.Scale(0.7f, 1.0f, 1.0f);
			//program.setModelMatrix(playerModelMatrix);
			modelMatrix.identity();
			modelMatrix.Translate(5.2f, -2.3f, 0.0f);
			modelMatrix.Scale(1.0f, 1.2f, 1.0f);
			program.setModelMatrix(modelMatrix);
			menuArt.Draw(&program);
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	Mix_FreeChunk(walkSound);
	Mix_FreeChunk(jumpSound);
	Mix_FreeMusic(music);
	SDL_Quit();
	return 0;
}