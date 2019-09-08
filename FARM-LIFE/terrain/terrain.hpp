#include <vector>
struct terraObj {
	// Store shader program and buffers
	GLuint terraShader;
	GLuint vbo;
	GLuint vao;
	GLuint ebo;
	GLuint tex;

	// Store terrain size and resolution
	float scale;
	int resX;
	int resZ;
	int noVertices;
} ;

terraObj createTerrain() {
	// Create terra object
	terraObj terra;

	// Initialise parameters for terrain size and resolution
	terra.resX = 500;
	terra.resZ = 500;
	terra.scale = 0.5;
	int vertexAtt = 8;

	// Create vertex array object and set it in terra
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	terra.vao = vao;

	//----------------
	// CREATE VERTICES
	//----------------

	float * vertices = new float[500 * 500 * 8];

	bool flag = true;	// for making the terrain diff colours

	// Constants for adjusting f(x,z) for the height of the terrain
	float c1 = 20, c2 = 0.02, c3 = 0.02;

	// Loop over grid
	for (int i = 0; i < terra.resX; i++) {
		for (int j = 0; j < terra.resZ; j++) {
			// Position
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 0] = (float)i;	// X
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 1] = c1 * glm::sin(i * 0.01 * cos(i * 0.01)) * glm::sin(j * 0.01 * cos(j * 0.01));		// Y
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 2] = (float)j;		// Z

			// Colour
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 3] = 0.0f;
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 4] = flag ? 0.3f : 0.8f;	// alternate greens
			flag = !flag;
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 5] = 0.0f;

			// Texture Coordinates
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 6] = i % 2 == 0 ? 0.0f : 1.0f;
			vertices[(i * (terra.resZ + terra.resZ * (vertexAtt - 1))) + (j * vertexAtt) + 7] = j % 2 == 0 ? 0.0f : 1.0f;
		}
	}

	//----------------
	// CREATE VBO ----
	//----------------

	// Create vertex buffer object
	// Bind vertices to buffer
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, terra.resX * terra.resZ * vertexAtt * sizeof(float), &vertices[0], GL_STATIC_DRAW);

	terra.vbo = vbo;

	delete[] vertices;	// free memory from vertices

	//-----------------
	// CREATE TRIANGLES
	//-----------------

	std::vector<GLuint> * triangles = new std::vector<GLuint>();

	// Loop mesh except last row and column
	for (int i = 0; i < terra.resX - 1; i++) {
		for (int j = 0; j < terra.resZ - 1; j++) {
			// Add vertices of triangles (two triangles that make a square across and up to current vertex)
			// Triangle 1 is bottom-left, top-left, top-right (i, j + 1), (i, j), (i, j + 1)
			triangles->push_back( ( ( i * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + ( ( j + 1 ) * vertexAtt) ) / vertexAtt);
			triangles->push_back( ( ( i * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + ( j * vertexAtt ) ) / vertexAtt);
			triangles->push_back( ( ( ( i + 1 ) * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + ( j * vertexAtt) ) / vertexAtt);
			
			// Triangle 2 is top-right, bottom-right, bottom-left (i + 1, j), (i + 1, j + 1), (i, j + 1)
			triangles->push_back( ( ( ( i + 1 ) * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + (j * vertexAtt) ) / vertexAtt);
			triangles->push_back( ( ( ( i + 1 ) * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + ( (j + 1 ) * vertexAtt) ) / vertexAtt);
			triangles->push_back( ( (i * ( terra.resZ + terra.resZ * (vertexAtt - 1) ) ) + ( ( j + 1 ) * vertexAtt) ) / vertexAtt);

			// Set the number of vertices for drawing later
			terra.noVertices = triangles->size();
		}
	}
	
	//----------------
	// CREATE EBO ----
	//----------------

	GLuint ebo;
	glGenBuffers(1, &ebo);

	// set elements to the triangles that were just made
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangles->size() * sizeof(float), &triangles->front(), GL_STATIC_DRAW);

	terra.ebo = ebo;

	std::vector<GLuint>().swap(*triangles);	// free memory from triangles

	//-----------------------------
	// CREATE SHADERS
	//-----------------------------

	// Create and link the vertex and fragment shaders
	GLuint shaderProgram = LoadShaders("terrain/terrain.vert", "terrain/terrain.frag");

	
	//--------------
	//TEXTURE BUFFER
	//--------------

	GLuint tex;
	glGenTextures(1, &tex);

	int width, height;

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex);

	unsigned char* image =
		SOIL_load_image("terrain/grass.jpg", &width, &height, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
		GL_UNSIGNED_BYTE, image);
	SOIL_free_image_data(image);
	glUniform1i(glGetUniformLocation(shaderProgram, "texGrass"), 0);

	//TODO: Set texture parameters with glTexParameteri(...)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glGenerateMipmap(GL_TEXTURE_2D);

	terra.tex = tex;

	//---------------------
	// LINK DATA TO SHADERS
	//---------------------
	GLint posAttrib = glGetAttribLocation(shaderProgram, "position");
	glEnableVertexAttribArray(posAttrib);
	glVertexAttribPointer(posAttrib, 3, GL_FLOAT, GL_FALSE,
		vertexAtt * sizeof(float), 0);

	GLint colAttrib = glGetAttribLocation(shaderProgram, "color");
	glEnableVertexAttribArray(colAttrib);
	glVertexAttribPointer(colAttrib, 3, GL_FLOAT, GL_FALSE,
		vertexAtt * sizeof(float), (void*)(3 * sizeof(float)));

	GLint texAttrib = glGetAttribLocation(shaderProgram, "texCoord");
	glEnableVertexAttribArray(texAttrib);
	glVertexAttribPointer(texAttrib, 2, GL_FLOAT, GL_FALSE,
		vertexAtt * sizeof(float), (void*)(6 * sizeof(float)));

	terra.terraShader = shaderProgram;


	return terra;
}

void generateTerrain(terraObj terra, glm::mat4 Hvw, glm::mat4 Hcv) {
	//-----------------------------
	// BIND SHADER AND BUFFERS ----
	//-----------------------------
	glUseProgram(terra.terraShader);
	glBindVertexArray(terra.vao);
	glBindBuffer(GL_ARRAY_BUFFER, terra.vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, terra.ebo);
	glBindBuffer(GL_TEXTURE_2D, terra.tex);
	
	//-----------------------------
	// SET CAMERA IN MIDDLE OF TERRAIN
	//-----------------------------
	glm::mat4 Hwm = glm::mat4(1.0f);
	Hwm[3] = glm::vec4(-(terra.resX * terra.scale ) / 2, -20.0, -(terra.resZ * terra.scale )/ 2 , 1.0);

	glUniformMatrix4fv(glGetUniformLocation(terra.terraShader, "Hvw"), 1, GL_FALSE, &Hvw[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(terra.terraShader, "Hcv"), 1, GL_FALSE, &Hcv[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(terra.terraShader, "Hwm"), 1, GL_FALSE, &Hwm[0][0]);
	glUniform1f(glGetUniformLocation(terra.terraShader, "scale"), terra.scale);

	std::cout << (Hcv * Hvw * Hwm)[3].length() << std::endl;

	//-----------------------------
	// DRAW TERRAIN
	//-----------------------------
	glDrawElements(GL_TRIANGLES, terra.noVertices, GL_UNSIGNED_INT, 0);
}