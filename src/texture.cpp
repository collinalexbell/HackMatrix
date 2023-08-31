#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <string>
#include "texture.h"
#include <glad/glad.h>
#include <iostream>


Texture::Texture(std::string fname, GLenum unit) {
  std::string ext = fname.substr(fname.find_last_of(".") + 1);
  GLenum textureFormat;
  if(ext == "png") {
    textureFormat = GL_RGBA;
  } else {
    textureFormat = GL_RGB;
  }
  glGenTextures(1, &ID);
  // need to provide texture binding for multi-texture work
  glActiveTexture(unit);
  glBindTexture(GL_TEXTURE_2D, ID);
  // set the texture wrapping/filtering options (on currently bound texture)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  stbi_set_flip_vertically_on_load(true);
  data = stbi_load(fname.c_str(), &width, &height, &nrChannels, 0);
  if (data)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, textureFormat,
                   GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  else
    {
      std::cout << "Failed to load texture" << std::endl;
    }
  stbi_image_free(data);
}
