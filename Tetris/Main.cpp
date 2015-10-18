/*Tetris
Author: Allan J Moore
Date: 18/10/2015
Description: Subset of features/ demonstration, just out of the blue wanted to write tetris
			 just in a single file, turned out slightly larger than planned, 
			 first time tetris in c++.

			This program requires the following libaries and corresponding headers to be linked: 
			SDL2.lib
			SDL2main.lib
			SDL2_image.lib
			glew32.lib
			opengl32.lib

			This program also requires the glm headers be included.

			The program was compiled and tested on windows 10, I can not guarentee the 
			program will compile sucessfully on other platforms, however it should be 
			possible without too much hassle.

			Semi-complete tileset included in GitHub repo
*/

#include <iostream>
#include <vector>

#include <GL/glew.h>
#include <GL/GL.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <random>
#include <time.h>

//Include the glm headers 
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/type_ptr.hpp>

//Define some useful types
typedef unsigned char uint8;
typedef signed char	sint8; 
typedef unsigned short uint16; 
typedef signed short sint16; 
typedef unsigned int uint32; 
typedef signed int sint32; 

class Vec2{
public:
	Vec2(int _x, int _y) { x = _x; y = _y; }
	signed int x; 
	signed int y; 
};

//!< Global Variables 
SDL_Window* window;
bool gameRunning = true; 

//!< Game properties 
Vec2 gridSize(12, 22);
Vec2 windowSize(800, 600);

//Block types enumeration 
/**
	Standard game of tetris consists of 7 blocks
	Naught == Empty 
*/
	//Inspiring names... 
enum BLOCK_TYPE{
	EMPTY = 0x0,
	BLOCK1 = 0x1 << 0, // Square 
	BLOCK2 = 0x1 << 1, // Line 
	BLOCK3 = 0x1 << 2, 
	BLOCK4 = 0x1 << 3, 
	BLOCK5 = 0x1 << 4, 
	BLOCK6 = 0x1 << 5,
	BLOCK7 = 0x1 << 6, 
	WALL = 0x1 << 7
};

//!< Array to hold grid data
uint8 grid[12 * 22];

//!< Array to hold blocks 
//!< Rather than come up with an eloborate algorithm I've stored the rotations within the arrays
//!< 7 blocks, 4 animations, 4x4 grid 
uint8 Block[7][16] = {
	
  { 1, 1, 0, 0, 
	1, 1, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0 },
	
	{0, 0, 0, 0,
	 1, 1, 1, 1,
	 0, 0, 0, 0,
	 0, 0, 0, 0},

	{1, 0, 0, 0,
	 1, 1, 1, 0,
	 0, 0, 0, 0,
	 0, 0, 0, 0 },

	{0, 0, 1, 0,
	 1, 1, 1, 0,
	 0, 0, 0, 0,
	 0, 0, 0, 0 },

	{0, 0, 0, 0,
	 1, 1, 1, 0,
	 0, 1, 0, 0,
	 0, 0, 0, 0 },

	{0, 1, 1, 0,
	 1, 1, 0, 0,
	 0, 0, 0, 0,
	 0, 0, 0, 0 },

	{1, 1, 0, 0,
	 0, 1, 1, 0,
	 0, 0, 0, 0,
	 0, 0, 0, 0 }

};

uint8 currentBlock[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
Vec2 blockPosition(0, 0);
uint8 currentBlockType = 0; 
uint8 currentBlockID = 0;

// The camera distance in the scene, hardcoded for simplicity, it is tetris afterall 
float camZDist = 0; 

// Define our view and projection martices 
glm::mat4 viewMat, projMat; 

// Buffer for grid outline
std::vector <float> gridLinesVertices; 
GLuint gridVBO;		//!< Vertex buffer object used by grid 
GLuint gridLinesID; 
GLuint gridPositions; //atrib pointer

// Buffers to hold vertex data for blocks
int _vertBufferSize;
int _texBufferSize;

std::vector <float>	vertices; //!< Holds vertex positions  
std::vector <float> textureCoords; //<! Holds texture coordinates 

// Buffer Ids 
GLuint vertexBufferID;	//!< Position buffer ID
GLuint texBufferID;		//!< Texture Coordinate Buffer ID

//Attribute locations 
GLint texAtribLocation; 

// Shader program we will use for EVERYTHING
GLuint program;

// Uniform locations 
GLint _useColour; 

// Textures 
GLuint blockTexture; 

//How fast the blocks drop ms
uint16 blockDropDefault = 600; // Level 1 value 
uint16 blockDropFaster = 30;
uint16 blockDropMS = 600; 
float blockDropedElapsed = 0; 
float previousTimeElapsed = 0; 

float inputElapsed = 0; 
float inputDelayMS = 0; 

// pre - declarations 
void rotateBlock(); 
void moveLeft(); 
void moveRight(); 
int loadTexture(const char* FilePath);

//Handles input
void poll(){

	inputElapsed += SDL_GetTicks() - previousTimeElapsed;
		
		SDL_Event _event;
		const Uint8 *keyState = SDL_GetKeyboardState(NULL);
		while (SDL_PollEvent(&_event)){
			//!< QUIT GAME
			if (keyState[SDL_SCANCODE_ESCAPE]){
				gameRunning = false;
			}

			if (keyState[SDL_SCANCODE_SPACE]){
				if (inputElapsed > inputDelayMS){
					inputElapsed = 0;
					rotateBlock();
				}
			}

			if (keyState[SDL_SCANCODE_LEFT]){
				moveLeft();
			}

			if (keyState[SDL_SCANCODE_RIGHT]){
				moveRight(); 
			}

			if (keyState[SDL_SCANCODE_DOWN]){
				blockDropMS = blockDropFaster;
			}
			else{
				//set drop speed back to current 
				blockDropMS = blockDropDefault; // - level * somevalue
			}
			
		}
	
}

void glCheck(){
	GLuint err = glGetError();
	if (err != GL_NO_ERROR)
		std::cerr << std::hex << err << std::endl;
}

void loadShaders(){
	GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
	GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

	std::string vertexShader =
		"#version 330 core \n "
		"layout (location = 0) in vec4 Vertex;"
		"layout (location = 1) in vec2 texCoordAtrib;"
		"uniform mat4 wvpMat;"
		//"out vec2 texCoord;"
		"varying vec2 texCoord;"
		"void main(){"
		"	texCoord = texCoordAtrib;"
		"	gl_Position = wvpMat * Vertex;"
		"}";

	std::string fragmentShader =
		"#version 330 core  \n "
		"uniform sampler2D tex;"
		"uniform bool useColour;"
		"in vec2 texCoord;"
		"layout (location = 0) out vec4 colour;"
		"void main(){"
		" if (useColour){colour = vec4(0,0,1,1);} else{colour = texture(tex, texCoord);}"
		"}";

	const char* vertSource = vertexShader.c_str();
	const char* fragSource = fragmentShader.c_str();

	glShaderSource(vShader, 1, &vertSource, NULL);
	glCompileShader(vShader);
	glShaderSource(fShader, 1, &fragSource, NULL); 
	glCompileShader(fShader);

	// Check if shaders compiled successfully 
	GLint success = 0;
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE){
		GLint logSize = 0;
		
		glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &logSize);
		char* log = new char[4000]; 
		GLsizei length;
		glGetShaderInfoLog(vShader, 4000, &length, log);
		std::cout << "Vertex Shader Failed: " << log<< std::endl; 
	}
	glGetShaderiv(fShader, GL_COMPILE_STATUS, &success);
	if (success == GL_FALSE){
		GLint logSize = 0;
		glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &logSize);
		char* log = new char[4000];
		GLsizei length;
		glGetShaderInfoLog(fShader, 4000, &length, log);
		std::cout << "Frag Shader Failed" << log << std::endl;
	}

	program = glCreateProgram(); 
	glAttachShader(program, vShader);
	glAttachShader(program, fShader);
	glLinkProgram(program);

	glUseProgram(program);


	glBindAttribLocation(program, 0, "Vertex");
	glBindAttribLocation(program, 1, "texCoordAtrib");

	GLint _wvpMat = glGetUniformLocation(program, "wvpMat");
	GLint _texture = glGetUniformLocation(program, "tex");
	_useColour = glGetUniformLocation(program, "useColour");

	glUniformMatrix4fv(_wvpMat, 1, false, glm::value_ptr(projMat * viewMat ));
	glUniform1i(_texture, 0);
	glUniform1i(_useColour, 0);


}

void init(){

	//Retrieve the window size 
	int w = 0, h = 0;
	SDL_GetWindowSize(window, &w, &h);

	glm::vec3 _position(0.0f, 0.0f, 20.0f);
	glm::vec3 _target(0.0f, 0.0f, 0.0f);
	glm::vec3 _up(0.0, 1.0f, 0.0f);

	// Initialise our view + projection matrices 
	viewMat = glm::lookAt(_position, _target, _up);

	projMat = glm::perspective(45.0f, (float)w / (float)h, 0.1f, 1000.0f);

	// Set viewport
	glViewport(0, 0, w, h);
	
	//Set grid to naught 
	for (int _g = 0; _g < gridSize.x*gridSize.y; _g++){
		grid[_g] = 0; 
	}

	//initialise grid lines array 

	// Resize vector for our data, each point is represented
	// (vertex)3 floats * 2 vertex per line * total gridlines 
	gridLinesVertices.resize((3 * 2)*(gridSize.x + gridSize.y));

	for (int _x = 0; _x < gridSize.x; _x++){
		gridLinesVertices[(_x * 6)] = (-(gridSize.x/2)) + _x;							
		gridLinesVertices[(_x * 6) + 1] = gridSize.y / 2;			
		gridLinesVertices[(_x * 6) + 2] = 0.0f;						
		gridLinesVertices[(_x * 6) + 3] = (-(gridSize.x / 2)) + _x;
		gridLinesVertices[(_x * 6) + 4] = -(gridSize.y / 2);		
		gridLinesVertices[(_x * 6) + 5] = 0.0f;						
	}
	for (int _y = 0; _y < gridSize.y; _y++){
		gridLinesVertices[(gridSize.x * 6) + (_y * 6)] = -(gridSize.x/2);							
		gridLinesVertices[(gridSize.x * 6) + (_y * 6) + 1] = (-(gridSize.y / 2)) + _y;		
		gridLinesVertices[(gridSize.x * 6) + (_y * 6) + 2] = 0.0f;					
		gridLinesVertices[(gridSize.x * 6) + (_y * 6) + 3] = gridSize.x / 2;
		gridLinesVertices[(gridSize.x * 6) + (_y * 6) + 4] = (-(gridSize.y / 2)) + _y;
		gridLinesVertices[(gridSize.x * 6) + (_y * 6) + 5] = 0.0f; 
	}

	glGenVertexArrays(1, &gridVBO);
	glBindVertexArray(gridVBO);

	glGenBuffers(1, &gridLinesID);
	glGenBuffers(1, &vertexBufferID);
	glGenBuffers(1, &texBufferID);

	glBindBuffer(GL_ARRAY_BUFFER, gridLinesID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * gridLinesVertices.size(), 
									&gridLinesVertices[0], GL_STATIC_DRAW);

	//load textures 
	blockTexture = loadTexture("blocks.png"); //!< Texture for all blocks 
	glBindTexture(GL_TEXTURE_2D, blockTexture);

	//Load our shaders 
	loadShaders();
	
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	vertices.resize(((gridSize.x*gridSize.y)*(3*6)));
	textureCoords.resize((gridSize.x*gridSize.y) * 12);
	std::fill(vertices.begin(), vertices.end(), 0);
	std::fill(textureCoords.begin(), textureCoords.end(), 0);
	
	// Set bricks at side 
	for (uint8 _gridY = 0; _gridY < gridSize.y; _gridY++){
		grid[_gridY*gridSize.x] = 8; 
		grid[(_gridY*gridSize.x) + (gridSize.x - 1)] = 8; 
	}

}

// Generates/ regenerates the buffer for the blocks in the grid 
void genBlockBuffer(){

	//Top left quardinates 
	Vec2 tLeft(-(float)gridSize.x / 2, (float)gridSize.y / 2);

	_vertBufferSize = 0; 
	_texBufferSize = 0; 
	
	uint16 _blockCoords[16];

	// Assign vertices for current block 
	for (uint8 p = 0; p < 4; p++){
		for (uint8 q = 0; q < 4; q++){
			if (currentBlock[(q * 4) + p] == 1){
				uint8 _gridX = blockPosition.x - 1 + p;
				uint8 _gridY = blockPosition.y - 1 + q;

				// If  outside grid bounds 
				if (_gridX < 0 || _gridY < 0)
					continue;

				float _texWidth = 1.0f / 8;
				float _texStartX = currentBlockID * _texWidth;

				// Every three represents a vertex 
				textureCoords[_texBufferSize + 0] = _texStartX;
				textureCoords[_texBufferSize + 1] = 1;
				textureCoords[_texBufferSize + 2] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 3] = 1;
				textureCoords[_texBufferSize + 4] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 5] = 0;
				textureCoords[_texBufferSize + 6] = _texStartX;
				textureCoords[_texBufferSize + 7] = 0;
				textureCoords[_texBufferSize + 8] = _texStartX;
				textureCoords[_texBufferSize + 9] = 1;
				textureCoords[_texBufferSize + 10] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 11] = 0;

				vertices[_vertBufferSize + 0] = tLeft.x + _gridX;
				vertices[_vertBufferSize + 1] = tLeft.y - _gridY;
				
				vertices[_vertBufferSize + 3] = tLeft.x + _gridX + 1;
				vertices[_vertBufferSize + 4] = tLeft.y - _gridY;

				vertices[_vertBufferSize + 6] = tLeft.x + _gridX + 1;
				vertices[_vertBufferSize + 7] = tLeft.y - _gridY - 1;

				vertices[_vertBufferSize + 9] = tLeft.x + _gridX;
				vertices[_vertBufferSize + 10] = tLeft.y - _gridY - 1;

				vertices[_vertBufferSize + 12] = tLeft.x + _gridX;
				vertices[_vertBufferSize + 13] = tLeft.y - _gridY;

				vertices[_vertBufferSize + 15] = tLeft.x + _gridX + 1;
				vertices[_vertBufferSize + 16] = tLeft.y - _gridY - 1;

				_vertBufferSize += 18;
				_texBufferSize += 12; 

			}

		}
	}

	// Calculate new buffer size and prepare vertex array
	for (int x = 0; x < gridSize.x; x++){
		for (int y = 0; y < gridSize.y; y++){

			if (grid[(y*gridSize.x) + x] > 0 ){
	
				float _texWidth = 1.0f / 8;
				float _texStartX = (grid[(y*gridSize.x) + x]-1) * _texWidth;

				// Every three represents a vertex 
				textureCoords[_texBufferSize + 0] = _texStartX;
				textureCoords[_texBufferSize + 1] = 1;
				textureCoords[_texBufferSize + 2] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 3] = 1;
				textureCoords[_texBufferSize + 4] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 5] = 0;
				textureCoords[_texBufferSize + 6] = _texStartX;
				textureCoords[_texBufferSize + 7] = 0;
				textureCoords[_texBufferSize + 8] = _texStartX;
				textureCoords[_texBufferSize + 9] = 1;
				textureCoords[_texBufferSize + 10] = _texStartX + _texWidth;
				textureCoords[_texBufferSize + 11] = 0;

				// Every three represents a vertex 
				vertices[_vertBufferSize + 0] = tLeft.x + x;
				vertices[_vertBufferSize + 1] = tLeft.y - y;

				vertices[_vertBufferSize + 3] = tLeft.x + x + 1;
				vertices[_vertBufferSize + 4] = tLeft.y - y;

				vertices[_vertBufferSize + 6] = tLeft.x + x + 1;
				vertices[_vertBufferSize + 7] = tLeft.y - y - 1;

				vertices[_vertBufferSize + 9] = tLeft.x + x;
				vertices[_vertBufferSize + 10] = tLeft.y - y - 1;

				vertices[_vertBufferSize + 12] = tLeft.x + x;
				vertices[_vertBufferSize + 13] = tLeft.y - y;

				vertices[_vertBufferSize + 15] = tLeft.x + x + 1;
				vertices[_vertBufferSize + 16] = tLeft.y - y - 1;

				_texBufferSize += 12;
				_vertBufferSize += 18;
			}
		}
	}
	
	glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (_vertBufferSize + _texBufferSize),
		0, GL_DYNAMIC_DRAW);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * _vertBufferSize, &vertices[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizeof(float) * _vertBufferSize, 
								sizeof(float) * _texBufferSize, &textureCoords[0]);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (GLvoid*)(sizeof(float)*_vertBufferSize));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);

}

// Randomonly select a block/ reset position to the top of the grid.
void newBlock(){
	//random number between 0 and 7
	uint8 _block = (rand() % 7);
	currentBlockType = (0x1 << (_block));
	currentBlockID = _block; 

	//copys the block to the current block array 
	for (uint8 i = 0; i < 16; i++){
		currentBlock[i] = Block[_block][i];
	}

	blockPosition = Vec2(gridSize.x/2, 0);
}

// Returns true if the current block is colliding with anything in grid.
bool checkCollision(){

	for (uint8 x = 0; x < 4; x++){
		for (uint8 y = 0; y < 4; y++){
			if (currentBlock[(y * 4) + x]){
				uint8 _gridX = blockPosition.x - 1 + x; 
				uint8 _gridY = blockPosition.y - 1 + y;

				if (_gridX < 0 || _gridY < 0 || _gridX >= gridSize.x || _gridY >= gridSize.y){
					return true; 
				}

				if (grid[(_gridY * gridSize.x) + _gridX]){
					return true; 
				}

			}
		}
	}

	return false; 


}

// Rotates the current block. 
void rotateBlock(){
	//rather than doing anything too elobarate 
	//square is not to rotate// line is speical case, everything else treated as a 3x3 matrix 

	// Is square block 
	if (currentBlockType & BLOCK_TYPE::BLOCK1){
		return; 
	}

	//Copy block to temp array 
	uint8 temp[16];
	for (uint8 i = 0; i < 16; i++)
		temp[i] = currentBlock[i];

	// Is line block 
	if (currentBlockType & BLOCK_TYPE::BLOCK2){
		// Hard coded solution is simpler 
		if (currentBlock[4] != 0){
			for (uint8 i = 0; i < 16; i++){
					currentBlock[i] = 0;
			}
			currentBlock[1] = 1; 
			currentBlock[5] = 1;
			currentBlock[9] = 1;
			currentBlock[13] = 1;
		}
		else{
			for (uint8 i = 0; i < 16; i++){
				currentBlock[i] = Block[1][i];
			}
		}
	
	}
	else if (currentBlockType >= BLOCK_TYPE::BLOCK6){ // Z blocks
		
		uint8 i = 0;

		if (temp[0] == 0 && temp[1] == 0){
			for (i = 0; i < 3; i++){
				for (uint8 p = 0; p < 3; p++)
					currentBlock[(p * 4) + i] = temp[((2 - i) * 4) + p];
			}
		}
		else{
			for (i = 0; i < 3; i++){
				for (uint8 p = 0; p < 3; p++)
					currentBlock[(p * 4) + i] = temp[((i) * 4) + (2 - p)];
			}
		}
	}
	else{	//!< every other block.

		uint8 i = 0;

		for (i = 0; i < 3; i++){
			for (uint8 p = 0; p < 3; p++)
				currentBlock[(p * 4) + i] = temp[((2-i) * 4) + p];
		}
	}

	// If collision after rotating, reset previous position
	if (checkCollision()){
		for (uint8 i = 0; i < 16; i++)
			currentBlock[i] = temp[i];
	}

}

void moveLeft(){

	bool _isColliding = false; 

	for (uint8 x = 0; x < 4; x++){
		for (uint8 y = 0; y < 4; y++){
			if (currentBlock[(y*4)+x]){
				if (blockPosition.x-2+x < 0 || blockPosition.y-1+y < 0){
					_isColliding = true; 
					break; 
				}

				if (grid[((blockPosition.y - 1 + y)*gridSize.x) + blockPosition.x - 2 + x]){
					_isColliding = true;
					break;
				}


			}
		}
	}
	if (!_isColliding){
		blockPosition.x -= 1;
	}
}

void moveRight(){
	bool _isColliding = false;

	for (uint8 x = 0; x < 4; x++){
		for (uint8 y = 0; y < 4; y++){
			if (currentBlock[(y * 4) + x]){
				if (blockPosition.x  + x >= gridSize.x || blockPosition.y - 1 + y > gridSize.y){
					_isColliding = true;
					break;
				}

				if (grid[((blockPosition.y - 1 + y)*gridSize.x) + blockPosition.x  + x]){
					_isColliding = true;
					break;
				}


			}
		}
	}
	if (!_isColliding){
		blockPosition.x += 1;
	}
}

//Remove a line by a given y coordinate
void removeLine(uint8 y){

	// Remove the enteries in grid#
	for (uint8 _x = 1; _x < gridSize.x - 1; _x++){
		grid[(y * gridSize.x) + _x] = 0; 
	}

	// Move all blocks above down by 1 
	for (uint8 _x = 1; _x < gridSize.x - 1; _x++){
		for (uint8 _y = y; gridSize.y - _y != 0; _y--){		
			// set to block above
			grid[(y * gridSize.x) + _x] = grid[((y - 1) * gridSize.x) + _x];
		}
	}

}

void checkLineComplete(){
	//Accounts for brick outline 
	for (uint8 _y = 0; _y < gridSize.y; _y++){
		bool lineComplete = true; 
		for (uint8 _x = 1; _x < gridSize.x-1; _x++){
			
			if (grid[(_y * gridSize.x) + _x] > 0){
				continue; 
			}
			else{
				lineComplete = false; 
				break; 
			}
		}

		if (lineComplete){
			removeLine(_y);
		}
	}
}

void dropDown(){
	// Check whether the block is about to collide, Note block pivot ==  1,1 
	// Which will cause a bit of confusion below I'm sure

	//Cordonites for top left of shapes new position 
	Vec2 topLeft = blockPosition;
	topLeft.x -= 1; 
	bool _collisionDetected = false;


	for (uint8 x = 0; x < 4; x++){
		for (uint8 y = 0; y < 4; y++){
			if (topLeft.x + x < 0 ){
				continue;
			}
			else if (topLeft.y + y >= gridSize.y){
				//check within bounds... 
				if (currentBlock[(4 * y) + x]){
					// Hit bottom of grid
					_collisionDetected = true;
					break; 
				}
			}



			if (currentBlock[(4 * y) + x])
				if (grid[((topLeft.y + y)*(gridSize.x)) + topLeft.x + x]){
					// Collision 
					_collisionDetected = true; 
					break; 
				}


		}
	}

	if (_collisionDetected){
		// Assign values in grid, create new block, return 
		for (uint8 x = 0; x < 4; x++)
			for (uint8 y = 0; y < 4; y++){
				if (currentBlock[(4 * y) + x] )
					grid[((blockPosition.y - 1 + y)*(gridSize.x)) + blockPosition.x - 1 + x] = currentBlockID + 1;
			}

		//Checks if any lines are complete. 
		checkLineComplete(); 
		newBlock();
	}
	else{
		blockPosition.y++;
	}
	
}

void update(){

	//glCheck();

	SDL_PumpEvents();
	float _currentTimeElapsed = SDL_GetTicks();
	blockDropedElapsed += (_currentTimeElapsed - previousTimeElapsed) ; 
	previousTimeElapsed = _currentTimeElapsed; 
	if (blockDropedElapsed > blockDropMS){
		blockDropedElapsed = 0; 
		dropDown(); 
	}

}

void render(){
	SDL_GL_SwapWindow(window);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindTexture(GL_TEXTURE_2D, blockTexture);

	/*glUniform1i(_useColour, 1);
	//!< Draw the grid 
	glBindBuffer(GL_ARRAY_BUFFER, gridLinesID);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glDrawArrays(GL_LINES, 0, gridLinesVertices.size()/3);*/

	glUniform1i(_useColour, 0);

	genBlockBuffer();
	glDrawArrays(GL_TRIANGLES, 0, _vertBufferSize/3);
	
}

// Loads texture and returns id
int loadTexture(const char* FilePath){
	
	SDL_Surface* image = IMG_Load(FilePath);
	if (image == NULL){
		std::cout<<("Unable to load texture?: Error: %s", SDL_GetError());
		return -1;
	}

	//get the opengl Texture + bind it. 
	GLuint texture;
	
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	/*
	uint32 _bufSize = (image->w * image->h);
	GLuint* pixels = new GLuint[_bufSize]();

	//Flips the pixel order 
	for (uint32 _p = 0; _p < _bufSize; _p++){
		pixels[_p] = ((GLuint *)image->pixels)[_bufSize - _p];

	}*/

	//Assuming theres no alpha layer format is set to GL_RGB

	//Texture info passed from sdl surface
	glTexImage2D(GL_TEXTURE_2D,
		0,//Mipmap Level
		GL_RGB, //bit per pixel
		image->w,
		image->h,
		0,//texture border 	
		GL_RGB,
		GL_UNSIGNED_BYTE,
		image->pixels);
	
	
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenerateMipmap(GL_TEXTURE_2D);
	//Clear memory
	SDL_FreeSurface(image);
	//delete[] pixels;

	return texture;//Returns the ID in opengl 

}

int main(int argc, char** argv){

	//!<Initialise SDL 
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		return 1;
	}

	IMG_Init(IMG_INIT_PNG);

	window = SDL_CreateWindow("Tetris", 100, 100, 800, 600, SDL_WINDOW_OPENGL);
	
	if (window == NULL)
		return 1; 

	SDL_GLContext glcontext;

	//!< Create the OpenGl Context for this window 
	glcontext = SDL_GL_CreateContext(window);

	glewInit();

	//!<OpenGl init.. 
	glClearColor(0, 0, 0, 1);
	

	gameRunning = true; 

	//Init the game // set up viewport matrices etc.. 
	init();

	// Create first block 
	newBlock(); 

	while (gameRunning){
		//!< Update the game 
		update(); 

		//!< Render the game 
		render(); 

		//!< Poll for input 
		poll();
	}

	//Clean up 
	glDeleteTextures(1, &blockTexture);

	//Destroy the OGL Context and window 
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(window);
	
	SDL_Quit(); 
	return 0; 

}