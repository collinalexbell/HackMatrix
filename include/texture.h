#ifndef __TEXTURE_H__
#define __TEXTURE_H__
#include <glad/glad.h>
#include <string>

using namespace std;

class Texture {
  int width, height, nrChannels;
  void loadTextureData(std::string);
  void initAndBindGlTexture(GLenum unit);
  void blankData();
 public:
  Texture(string, GLenum unit);
  Texture(GLenum unit);
  unsigned int ID;

};

#endif
