#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "texture.h"
#include <glad/glad.h>
#include <iostream>


Texture::Texture(const char* fname){
  glGenTextures(1, &ID);
  // need to provide texture binding for multi-texture work
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ID);
  // set the texture wrapping/filtering options (on currently bound texture)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  data = stbi_load(fname, &width, &height, &nrChannels, 0);
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
