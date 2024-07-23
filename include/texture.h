#ifndef __TEXTURE_H__
#define __TEXTURE_H__
#include <glad/glad.h>
#include <string>
#include <vector>

using namespace std;

class Texture
{
  int width, height, nrChannels;
  void loadTextureData(std::string);
  void loadTextureArrayData(vector<string> fnames);
  void initAndBindGlTexture(GLenum unit);
  void initAndBindGlTextureArray(GLenum unit);
  void blankData();

public:
  Texture(string, GLenum unit);
  Texture(vector<string>, GLenum);
  Texture(GLenum unit);
  unsigned int ID;
};

#endif
