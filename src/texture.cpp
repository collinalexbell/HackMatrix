#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <string>
#include "texture.h"
#include <glad/glad.h>
#include <iostream>

//------------
// Helpers
//------------

GLenum textureFormat(std::string fname) {
  std::string ext = fname.substr(fname.find_last_of(".") + 1);
  GLenum rv;
  if(ext == "png") {
    rv = GL_RGB;
  } else {
    rv = GL_RGB;
  }
  return rv;
}

void setTextureParameters(bool mipmapped , bool pixelated) {
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  if(pixelated) {
    if(mipmapped) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    } else {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
  } else {
    if(mipmapped) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
  }
}

//------------
// Texture::*
//------------

void Texture::loadTextureData(std::string fname) {
  unsigned char* data = stbi_load(fname.c_str(), &width, &height, &nrChannels, 0);
  if (data)
    {
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, textureFormat(fname),
                   GL_UNSIGNED_BYTE, data);
      glGenerateMipmap(GL_TEXTURE_2D);
    }
  else
    {
      std::cout << "Failed to load texture" << std::endl;
    }
  stbi_image_free(data);
}

void Texture::blankData() {
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
}

void Texture::initAndBindGlTexture(GLenum unit) {
  glGenTextures(1, &ID);
  glActiveTexture(unit);
  glBindTexture(GL_TEXTURE_2D, ID);
}

Texture::Texture(std::string fname, GLenum unit) {
  initAndBindGlTexture(unit);
  setTextureParameters(true, true);
  stbi_set_flip_vertically_on_load(true);
  loadTextureData(fname);
}


Texture::Texture(GLenum unit) {
  initAndBindGlTexture(unit);
  setTextureParameters(false, false);
  blankData();
}
