#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "texture.h"
#include <glad/glad.h>
#include <iostream>

Texture::Texture(const char* fname){
  data = stbi_load(fname, &width, &height, &nrChannels, 0);
  glGenTextures(1, &ID);
  glBindTexture(GL_TEXTURE_2D, ID);
  if (data)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                   GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  else
    {
      std::cout << "Failed to load texture" << std::endl;
    }
  stbi_image_free(data);
}
