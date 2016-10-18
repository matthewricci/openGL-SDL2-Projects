/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 4 - Space Invaders remake
NOTES: features missing - make start text bob up and down, arrow keys for motion, implement bullets, implement collision detection for bullets, enemies don't yet move.
	IF COLLISION IS MESSED UP: keep in mind you're scaling the player ship by 0.8 after its width/height is set
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
		sprite(iSprite), position(iPosition), size(iSize), alive(true){}
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
	SheetSprite blueShip(sprites, 0.0f, 0.0f, 422.0f / 1024.0f, 372.0f / 512.0f, 1.0f);	 //initializing textures from the spritesheet
	Entity player(blueShip, initializer, initializer, initializer);

	//vector to hold all enemy entities
	std::vector<Entity> enemies;
	SheetSprite enemySprite(sprites, 424.0f/1024.0f, 0.0f, 324.0f / 1024.0f, 340.0f / 512.0f, 0.5f);
	for (size_t i = 0; i < 30; i++){
		Entity enemy(enemySprite, initializer, initializer, initializer);  //initializer defined where player ship created
		enemy.size.x = enemy.sprite.size;  // is this right? ***** times aspect
		enemy.size.y = enemy.sprite.size;  // is this right? *****
		enemies.push_back(enemy);
	}

	//creating an object pool for the player's bullets
#define MAX_BULLETS 5
	int blueBulletIndex = 0;
	SheetSprite blueBulletSprite(sprites, 0.0f, 374.0f / 512.0f, 9.0f / 1024.0f, 37.0f / 512.0f, 0.5f);
	std::vector<Entity> blueBulletPool;
	for (size_t i = 0; i < MAX_BULLETS; i++){
		Entity blueBullet(blueBulletSprite, initializer, initializer, initializer);
		blueBullet.size.x = blueBulletSprite.width * blueBulletSprite.size;
		blueBullet.size.y = blueBulletSprite.height * blueBulletSprite.size;
		blueBullet.position.x = -2000.0f;
		blueBullet.alive = false;
		blueBulletPool.push_back(blueBullet);
	}

	//start-screen text
	std::string first_line = "~ Bootleg Invaders ~";
	std::string second_line = "Press space to begin the adventure!";


	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			//else if (event.type == SDL_KEYDOWN){
			//	if (event.key.keysym.scancode == SDL_SCANCODE_SPACE){    //press space to start the ball
			//		if (!start){
			//			start = true;
			//		}
			//	}
			//}
		}

		//for general time-keeping between frames
		float ticks = (float)SDL_GetTicks() / 1000.0f;
		std::cout << SDL_GetTicks() << std::endl;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		//start game by pressing space
		if (state == START_SCREEN && keys[SDL_SCANCODE_SPACE])
			state = LEVEL_ONE;
		if (state == LEVEL_ONE && keys[SDL_SCANCODE_LEFT])
			player.position.x -= 1.5f * elapsed;
		if (state == LEVEL_ONE && keys[SDL_SCANCODE_RIGHT])
			player.position.x += 1.5f * elapsed;

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
			modelMatrix.identity();
			modelMatrix.Translate(player.position.x, -1.0f, 0.0f);
			modelMatrix.Scale(1, 0.8, 1);
			program.setModelMatrix(modelMatrix);
			blueShip.Draw(&program);
			if (state == LEVEL_ONE && keys[SDL_SCANCODE_SPACE]){
				blueBulletPool[blueBulletIndex].position.x = player.position.x;
				blueBulletPool[blueBulletIndex].position.y = player.position.y;
				blueBulletPool[blueBulletIndex].Draw(&program);
				blueBulletPool[blueBulletIndex].alive = true;
				if (blueBulletIndex < 4)
					blueBulletIndex++;
				else
					blueBulletIndex = 0;
			}

			for (size_t i = 0; i < blueBulletPool.size(); i++){
				if (blueBulletPool[i].alive){
					modelMatrix.identity();
					modelMatrix.Translate(blueBulletPool[i].position.x, blueBulletPool[i].position.y, 0.0f);
					blueBulletPool[i].Draw(&program);
				}

			}

			float spacing = 0.0f;				//initialize space btween enemies
			for (size_t i = 0; i < 5; i++){
				if (enemies[i].alive){
					modelMatrix.identity();
					modelMatrix.Translate(-1.5f + spacing, 0.5f, 0.0f);		// translate starting from as far as -1.5f
					modelMatrix.Scale(1.5, 1, 1);
					program.setModelMatrix(modelMatrix);
					enemies[i].Draw(&program);
					spacing += enemies[i].size.x * 1.25;					// increment spacing by the size of the enemy last drawn, scaled by 1.25
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