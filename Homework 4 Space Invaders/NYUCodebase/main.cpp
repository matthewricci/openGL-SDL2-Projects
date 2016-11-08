/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 4 - Space Invaders remake
NOTES: I didn't get to finish this project. I have most of it done, but the collision detection is completely wrong.
	I would need help/more time to look through the code to fix it. However, the two game states are there, the game
	employs a two-state system, and uses sprite sheets. The bullets are stored via object pools.

	Press space to start, press space to shoot.
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

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

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
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640, 360);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint background = LoadTexture("Space Background.png");
	GLuint sprites = LoadTexture("sprites.png");
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

	//creating an object pool for the player's bullets
#define MAX_BULLETS 10
	
	//initialize indexes
	int blueBulletIndex = 0;
	int redBulletIndex = 0;

	//initialize bullet sprites
	SheetSprite blueBulletSprite(sprites, 0.0f, 374.0f / 512.0f, 9.0f / 1024.0f, 37.0f / 512.0f, 0.2f);
	SheetSprite redBulletSprite(sprites, 0.0f, 413.0f / 512.0f, 9.0f / 1024.0f, 37.0f / 512.0f, 0.2f);

	//define object pool for player's bullets
	std::vector<bulletEntity> blueBulletPool;
	for (size_t i = 0; i < MAX_BULLETS; i++){
		bulletEntity blueBullet(blueBulletSprite, initializer, initializer, initializer);
		blueBullet.size.x = (blueBulletSprite.width * blueBulletSprite.height) * blueBulletSprite.size; // mult. by aspect ratio
		//blueBullet.size.x = 0.5f;
		blueBullet.size.y = blueBulletSprite.size;
		blueBullet.position.x = -2000.0f;
		blueBullet.alive = false;
		blueBulletPool.push_back(blueBullet);
	}
	//define object pool for enemy's bullets
	std::vector<bulletEntity> redBulletPool;
	for (size_t i = 0; i < MAX_BULLETS; i++){
		bulletEntity redBullet(redBulletSprite, initializer, initializer, initializer);
		redBullet.size.x = (redBulletSprite.width * redBulletSprite.height) * redBulletSprite.size; // mult. by aspect ratio
		redBullet.size.y = redBulletSprite.size;
		redBullet.alive = false;
		redBulletPool.push_back(redBullet);
	}

	//start-screen text and game level score text
	std::string first_line = "~ Bootleg Invaders ~";
	std::string second_line = "Press space to begin the adventure!";
	int points = 0;
	std::string score = "Score: " + std::to_string(points);

	float spacing = 0.0f;				//initialize space btween enemies
	float distance = 0.0f;				//tracks
	float x = 2.0f;

	//used as a time counter for enemy movement
	float timeCount = 0.0f;

	//used to space out player's shots
	float lastShotEnemy = 0.0f;

	//used to space out enemy's shots
	float lastShotPlayer = 0.0f;

	while (!done) {
		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		timeCount += elapsed;
		lastShotEnemy += elapsed;		//buffers the player's shots to at most once a second
		lastShotPlayer += elapsed;		//buffers the enemy's shots to exactly once a second

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN){
				if (state == LEVEL_ONE && event.key.keysym.scancode == SDL_SCANCODE_SPACE){    //press space to shoot
					if (lastShotEnemy > 1.0f){	//wait 1 second between shots
						lastShotEnemy = 0.0f;	//reset lastShot timer
						blueBulletPool[blueBulletIndex].position.x = player.position.x;
						blueBulletPool[blueBulletIndex].position.y = player.position.y + player.size.y/2;
						blueBulletPool[blueBulletIndex].alive = true;
						blueBulletPool[blueBulletIndex].velocity.y = 2.5f;
						if (blueBulletIndex < 9)
							blueBulletIndex++;
						else
							blueBulletIndex = 0;
					}
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
			if (player.position.x < -3.4f)
				player.position.x += 0.02f;
			else
				player.position.x -= 1.5f * elapsed;
		}
		if (state == LEVEL_ONE && keys[SDL_SCANCODE_RIGHT]){
			if (player.position.x > 3.4f)
				player.position.x -= 0.02f;
			else
				player.position.x += 1.5f * elapsed;
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
			modelMatrix.Translate(-3.25f, 1.7f, 0.0f);
			drawText(&program, font, score, 0.18f, 0.0f, &modelMatrix);

			//draw the player
			if (player.alive){
				modelMatrix.identity();
				modelMatrix.Translate(player.position.x, player.position.y, player.position.z);
				//modelMatrix.Scale(1, 0.8, 1);
				program.setModelMatrix(modelMatrix);
				blueShip.Draw(&program);
			}

			//draw all active bullets
			for (size_t i = 0; i < blueBulletPool.size(); i++){
				if (blueBulletPool[i].alive){
					modelMatrix.identity();
					blueBulletPool[i].position.y += blueBulletPool[i].velocity.y * elapsed;
					modelMatrix.Translate(blueBulletPool[i].position.x, blueBulletPool[i].position.y, 0.0f);
					program.setModelMatrix(modelMatrix);
					blueBulletPool[i].Draw(&program);
					if (blueBulletPool[i].timeAlive > 2.5f){	// if bullet shot more than 2 secs ago
						blueBulletPool[i].timeAlive = 0.0f;		//get rid of it
						blueBulletPool[i].alive = false;
					}
					else {
						blueBulletPool[i].timeAlive += elapsed;  //else add elapsed time to its tracker
						for (size_t i = 0; i < enemies.size(); i++){
							if (enemies[i].alive){
								if (detectCollision(&blueBulletPool[i], &enemies[i])){
									blueBulletPool[i].alive = false;
									enemies[i].alive = false;
									points += 100;
									score = "Score: " + std::to_string(points);
								}
							}
						}
					}
				}

			}

			float spacing = 0.0f;				//initialize space btween enemies
			if (distance > 1.5f)
				x = -0.5f;
			if (distance < -1.5f)
				x = 0.5f;
			distance += x * elapsed;
			for (size_t i = 0; i < enemies.size(); i++){
				spacing += enemies[i].size.x * 3;  // increment spacing by the size of the enemy last drawn, scaled by 3
				if (enemies[i].alive){
					//enemies[i].position.x += x;
					enemies[i].position.x = (-1.5 * enemies[i].size.x * enemies.size()) + spacing + distance;
					//enemies[i].position.x = 
					modelMatrix.identity();
					modelMatrix.Translate(enemies[i].position.x, enemies[i].position.y, 0.0f);		// translate starting from as far as -1.5f
					//modelMatrix.Scale(1.5, 1, 1);
					program.setModelMatrix(modelMatrix);
					enemies[i].Draw(&program);
				}
			}

			//code for shooting back at player
			if (lastShotPlayer > 1.0f){		//if it has been 1 second since last enemy shot
				size_t enemyShooter = std::rand() % (enemies.size());		//get a random enemy from array and shoot
				while (!enemies[enemyShooter].alive)		//while chosen enemy is not alive, find new enemy
					enemyShooter = std::rand() % (enemies.size());
				//size_t enemyShooter = 1;
				lastShotPlayer = 0.0f;	//reset lastShotPlayer timer
				redBulletPool[redBulletIndex].position.x = enemies[enemyShooter].position.x;
				redBulletPool[redBulletIndex].position.y = enemies[enemyShooter].position.y - enemies[enemyShooter].size.y/4;
				redBulletPool[redBulletIndex].alive = true;
				redBulletPool[redBulletIndex].velocity.y = -2.5f;
				if (redBulletIndex < 9)
					redBulletIndex++;
				else
					redBulletIndex = 0;
			}
			for (size_t i = 0; i < redBulletPool.size(); i++){
				if (redBulletPool[i].alive){
					redBulletPool[i].position.y += redBulletPool[i].velocity.y * elapsed;
					if (detectCollision(&redBulletPool[i], &player)){
						if (player.lives == 0)
							player.alive = false;
						else
							player.lives--;
					}
					modelMatrix.identity();
					modelMatrix.Translate(redBulletPool[i].position.x, redBulletPool[i].position.y, 0.0f);
					program.setModelMatrix(modelMatrix);
					//redBulletPool[i].sprite = redBulletSprite;
					redBulletPool[i].Draw(&program);
					if (redBulletPool[i].timeAlive > 2.5f){	// if bullet shot more than 2 secs ago
						redBulletPool[i].timeAlive = 0.0f;		//get rid of it
						redBulletPool[i].alive = false;
					}
					else {
						redBulletPool[i].timeAlive += elapsed;  //else add elapsed time to its tracker
					}
				}
			}


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