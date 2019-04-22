float vertices[] = {
   // positions           // colors           // uvs
   200.0f,  120.0f,  0.0f,   0.0f, 1.0f, 1.0f,   1.0f, 1.0f,
   200.0f, -120.0f,  0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
  -200.0f, -120.0f,  0.0f,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
  -200.0f,  120.0f,  0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f
};

GLint indices[] = {
  0, 1, 3,
  1, 2, 3
};

void
SetTestPlaneForRendering()
{
  /*
   * VAO - combines VBOs into one object.
   */
  glGenVertexArrays(
    1,       // vertex array object names count to generate
    &glAtom.vao// array of object names
  );
  // Make this VAO the active one (remember OpenGL is a state machine).
  // You can have different VBO and VAO active. This bind has only one argument
  // because VAO can have only one role - it's more of a wrapper.
  glBindVertexArray(glAtom.vao);

  /*
   * VBO - represents a single attribute. Those aren't usually rendered
   * directly.
   */
  glGenBuffers(
    1,                        // buffer object names count to generate
    &glAtom.vboVertices      // array of object names
  );

  // Make this VBO the active one (remember OpenGL is a state machine).
  // You can have different VBO and VAO active. First argument specifies
  // the role of this buffer.
  glBindBuffer(GL_ARRAY_BUFFER, glAtom.vboVertices);
  // describe the data in the buffer
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  /*
   * EBO
   */
  glGenBuffers(
    1,
    &glAtom.eboIndices
  );
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glAtom.eboIndices);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (GLvoid*)indices, GL_STATIC_DRAW);

  /*
   * TEXTURES
   */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glGenTextures(1, &glAtom.texture);
  glBindTexture(GL_TEXTURE_2D, glAtom.texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
               FONT_TEST_IMAGE_WIDTH, FONT_TEST_IMAGE_HEIGHT,
               0, GL_RED, GL_UNSIGNED_BYTE,
               image);
  glGenerateMipmap(GL_TEXTURE_2D);
  // stbi_image_free(image);
}
