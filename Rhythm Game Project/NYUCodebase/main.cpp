/*
NAME: Matthew Ricci
*/

#ifdef _WINDOWS
	#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "Matrix.h"
#include "ShaderProgram.h"

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

enum Position{GREEN, RED, YELLOW, BLUE, ORANGE};

SDL_Window* displayWindow;

class Vector{
public:
	Vector(float x = 0.0f, float y = 0.0f) : x(x), y(y){}

	float x;
	float y;
};

class Note {
public:
	Note(GLuint noteTexture, Position color) : noteTexture(noteTexture), position(Vector(0.02f, 0.42f)), velocity(Vector(0.0f, 0.5f)) {
		if (color == YELLOW){
			startingPosition = 0.02f;
			position.x = startingPosition;
			velocity.x = 0.005f;

		}
	}

	void Update(float timeElapsed){
		position.y -= velocity.y * timeElapsed;
		position.x += velocity.x * timeElapsed;

		if (position.y < -1.75f){
			position.y = 0.42f;
			position.x = startingPosition;
		}
	}

	void Draw(ShaderProgram *program, Matrix &modelMatrix){
		glBindTexture(GL_TEXTURE_2D, noteTexture);
		modelMatrix.identity();
		modelMatrix.Translate(position.x, position.y, 0.0f);
		modelMatrix.Scale(0.33f, 0.25f, 1.0f);
		program->setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	GLuint noteTexture;
	Position color;
	float startingPosition = 0.02f;
	Vector position;
	Vector velocity;
	float lifetime;
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

void drawBackground(ShaderProgram *program, GLuint background, Matrix &modelMatrix){
	glBindTexture(GL_TEXTURE_2D, background);
	modelMatrix.identity();
	modelMatrix.Scale(7.0f, 4.0f, 1.0f);
	program->setModelMatrix(modelMatrix);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*2, 360*2, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

	SDL_Event event;
	bool done = false;
	
	//setting default color
	glClearColor(0.1f, 0.0f, 0.0f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640*2, 360*2);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint board = LoadTexture("images/board.png");
	GLuint note = LoadTexture("images/note.png");
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

	Note a(note, YELLOW);

	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);
	

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		std::cout << SDL_GetTicks() << std::endl;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		

		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);

		//drawing board every frame in the same place
		drawBackground(&program, board, modelMatrix);

		a.Update(elapsed);
		a.Draw(&program, modelMatrix);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	SDL_Quit();
	return 0;
}
