/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 2 - 2D animation
NOTES: The following works and I don't know why. It creates a 2D scene showing two Toads from Mario Bros. gawking at a spinning star.
This is what I wanted the animation to be, but I don't understand why it works this way, particularly because the texture transformations
do not match up with the texture binds as they are called in the while loop.  
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

SDL_Window* displayWindow;

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
	glClearColor(0.2f, 0.8f, 0.4f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640, 360);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint toadLeft = LoadTexture("toadLeft.png");
	GLuint toadRight = LoadTexture("toadRight.png");
	GLuint star = LoadTexture("star.png");
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

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

		//code for binding, drawing, and transforming star
		glBindTexture(GL_TEXTURE_2D, star);
		modelMatrix.identity();
		modelMatrix.Translate(-1.5f, -0.5f, 0.0f);
		modelMatrix.Scale(1.2f, 1.2f, 0.0f); //needed to be a bit bigger to make symmetrical
		glDrawArrays(GL_TRIANGLES, 0, 6);

		program.setModelMatrix(modelMatrix);

		//code for binding, drawing, and transforming toadLeft
		program.setModelMatrix(modelMatrix);  //needed to hand off modelMatrix to toadRight texture
		modelMatrix.identity();				//resets modelMatrix to center for new transformations
		glBindTexture(GL_TEXTURE_2D, toadLeft);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		modelMatrix.identity();				//resets modelMatrix to center for new transformations
		modelMatrix.Translate(1.5f, -0.5f, 0.0f);

		//code for binding, drawing, and animating toadRight
		program.setModelMatrix(modelMatrix);
		glBindTexture(GL_TEXTURE_2D, toadRight);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		modelMatrix.identity();
		modelMatrix.Translate(0.1f, 1.2f, 0.0f);
		modelMatrix.Rotate(ticks * 300 * (3.1415926 / 180.0));
		program.setModelMatrix(modelMatrix);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	SDL_Quit();
	return 0;
}
