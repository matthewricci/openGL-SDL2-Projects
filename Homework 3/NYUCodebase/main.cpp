/*
NAME: Matthew Ricci
CLASS: CS3113 Homework 3 - Pong Remake
NOTES: Press spacebar to start the game! The ball is represented by a rotating star (taken from my HW2 animation).
	   The left padde is controlled with W and S, the right paddle is controlled with the Up and Down arrows.
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
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);

	//texture coordinates
	glViewport(0, 0, 640, 360);
	ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glUseProgram(program.programID);
	GLuint star = LoadTexture("star.png");
	GLuint square = LoadTexture("square.png");
	Matrix projectionMatrix;
	Matrix modelMatrix;
	Matrix viewMatrix;

	projectionMatrix.setOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);

	//for keeping time
	float lastFrameTicks = 0.0f;

	//for starting game
	bool start = false;

	//for movement and positioning
	float leftPaddleMove = 0.0f;
	float leftPosition = 0.0f;

	float rightPaddleMove = 0.0f;
	float rightPosition = 0.0f;

	float ballPositionX = 0.0f;
	float ballPositionY = 0.0f;
	float ballMoveX = 0.0f;
	float ballMoveY = 0.0f;

	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
			else if (event.type == SDL_KEYDOWN){
				if (event.key.keysym.scancode == SDL_SCANCODE_SPACE){    //press space to start the ball
					if (!start){
						start = true;
						ballMoveX = 4.0f;
						ballMoveY = 1.0f;
					}
				}
			}
		}
		const Uint8 *keys = SDL_GetKeyboardState(NULL);

		//left paddle movement calculation
		if (keys[SDL_SCANCODE_W])
			if (leftPosition < 1.4f)
				leftPaddleMove = 2.0f;
			else
				leftPaddleMove = 0.0f;
		else if (keys[SDL_SCANCODE_S])
			if (leftPosition > -1.4f)
				leftPaddleMove = -2.0f;
			else
				leftPaddleMove = 0.0f;
		else
			leftPaddleMove = 0.0f;

		//right paddle movement calculation
		if (keys[SDL_SCANCODE_UP])        // if up is pressed
			if (rightPosition < 1.4f)     // if paddle is below 1.4f
				rightPaddleMove = 2.0f;   // movement speed gets activated
			else
				rightPaddleMove = 0.0f;   // otherwise movement speed deactivated
		else if (keys[SDL_SCANCODE_DOWN]) // if down is pressed
			if (rightPosition > -1.4f)    // if paddle is above -1.4f
				rightPaddleMove = -2.0f;  // movement speed gets activated
			else
				rightPaddleMove = 0.0f;   // otherwise movement speed deactivated
		else                              
			rightPaddleMove = 0.0f;       // if no keys, movement defaults to zero

		glClear(GL_COLOR_BUFFER_BIT);


		float ticks = (float)SDL_GetTicks() / 1000.0f;
		std::cout << SDL_GetTicks() << std::endl;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5,  };

		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float texCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		//setting up matrixes
		program.setModelMatrix(modelMatrix);
		program.setProjectionMatrix(projectionMatrix);
		program.setViewMatrix(viewMatrix);

		//static/background objects drawn here
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glBindTexture(GL_TEXTURE_2D, square);
		//top-bar transformations
		modelMatrix.identity();
		modelMatrix.Translate(0.0f, 1.9f, 0.0f);
		modelMatrix.Scale(10.0f, 0.2f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		//bottom-bar transformations
		modelMatrix.identity();
		modelMatrix.Translate(0.0f, -1.9f, 0.0f);
		modelMatrix.Scale(10.0f, 0.2f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		//center-line transformations
		modelMatrix.identity();
		modelMatrix.Scale(0.05f, 4.0f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		//left paddle drawing
		leftPosition += leftPaddleMove * elapsed;
		modelMatrix.identity();
		modelMatrix.Translate(-3.3f, leftPosition, 0.0f);
		modelMatrix.Scale(0.1f, 0.8f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		//right paddle drawing
		rightPosition += rightPaddleMove * elapsed;
		modelMatrix.identity();
		modelMatrix.Translate(3.3f, rightPosition, 0.0f);
		modelMatrix.Scale(0.1f, 0.8f, 1.0f);
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		//ball drawing
		glBindTexture(GL_TEXTURE_2D, star);
		modelMatrix.identity();
		if (ballPositionX > 3.3f){		// if passing right paddle
			if (rightPosition-0.55f  <= ballPositionY && ballPositionY <= rightPosition+0.55f){  //if paddle is present, bounce
				ballPositionX = 3.25f;
				ballMoveX *= -1.0f;
			} 
		}
		else if (ballPositionX < -3.3f){		//if passing left paddle
			if (leftPosition-0.55f <= ballPositionY && ballPositionY <= leftPosition+0.55f){		//if paddle is present, bounce
				ballPositionX = -3.25f;
				ballMoveX *= -1.0f;
			}
		}
		if (ballPositionX < -4.0f || ballPositionX > 4.0f) {	// else game over, restart!
			ballPositionX = 0.0f;
			ballPositionY = 0.0f;
			ballMoveX = 0.0f;
			ballMoveY = 0.0f;
			start = false;
		}
		if (ballPositionY > 1.85f){   // collision with the top bar
			ballPositionY = 1.70f;
			ballMoveY *= -1.0f;
		}
		else if (ballPositionY < -1.85f){	// collision with the bottom bar
			ballPositionY = -1.70f;
			ballMoveY *= -1.0f;
		}
		ballPositionX += ballMoveX * elapsed;
		ballPositionY += ballMoveY * elapsed;
		modelMatrix.Translate(ballPositionX, ballPositionY, 0.0f);
		modelMatrix.Scale(0.4f, 0.4f, 1.0f);
		modelMatrix.Rotate(ticks * 300 * (3.1415926 / 180.0));
		program.setModelMatrix(modelMatrix);
		glDrawArrays(GL_TRIANGLES, 0, 6);



		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.texCoordAttribute);

		SDL_GL_SwapWindow(displayWindow);

	}
	SDL_Quit();
	return 0;
}